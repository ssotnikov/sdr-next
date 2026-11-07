#include "engine.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <iostream>
#include <sdr/path.hpp>
#include <stdexcept>
namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void wait(F predicate, const char *message) {
    for (int i = 0; i < 2000; ++i) {
        if (predicate())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error(message);
}
void write(const QString &path, const QJsonObject &m) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "Manifest writable");
    check(file.write(QJsonDocument(m).toJson()) > 0, "Manifest written");
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("sdr-device-engine-tests");
    QStandardPaths::setTestModeEnabled(true);
    try {
        check(argc == 4, "Provider fixture arguments");
        QTemporaryDir root;
        check(root.isValid(), "Temporary folder");
        const auto directory = root.filePath("devices");
        AudioBuffer audio;
        Engine engine(audio, directory);
        wait([&] { return engine.snapshot().devices.size() == 2; },
             "Software sources with no module directory");
        check(engine.snapshot().device_modules.empty() && engine.snapshot().error.isEmpty(),
              "No modules is not a global error");
        sdr::Config config;
        config.sample_rate = 240000;
        config.mode = "AM";
        engine.post([&] { engine.start("synthetic", config); });
        wait([&] { return engine.snapshot().running && engine.snapshot().iq_samples > 0; },
             "Synthetic RX without hardware modules");
        engine.post([&] { engine.stop(); });
        wait([&] { return !engine.snapshot().running; }, "Software RX stopped");
        const auto iq = root.filePath("software.cf32");
        sdr::IqWriter writer;
        writer.open(sdr::path_from_utf8(iq.toUtf8().toStdString()));
        writer.write(std::vector<sdr::IQ>(sdr::block_size * 2, {0.5f, 0}));
        writer.close();
        config.file = iq.toUtf8().toStdString();
        engine.post([&] { engine.start("file", config); });
        wait([&] { return engine.snapshot().running; }, "File RX without hardware modules");
        wait([&] { return !engine.snapshot().running; }, "File EOF without hardware modules");
        check(engine.snapshot().error.isEmpty(), "Software/file fallback has no module error");
        check(QDir().mkpath(directory), "Module folder");
        auto install = [&](int argument) {
            const auto file = QString::fromLocal8Bit(argv[argument]);
            const auto name = QFileInfo(file).fileName();
            check(QFile::copy(file, QDir(directory).filePath(name)), "Fixture library copied");
            return name;
        };
        const auto library = install(1), bad = install(2), unavailable = install(3);
        QJsonObject manifest{{"id", "org.sdr.example.device"},
                             {"version", "0.1.0"},
                             {"api_major", 1},
                             {"api_minor", 0},
                             {"kind", "device"},
#ifdef Q_OS_WIN
                             {"platform", "Windows"},
#else
                             {"platform", "Linux"},
#endif
                             {"arch", QSysInfo::currentCpuArchitecture()},
                             {"license", "MIT"},
                             {"library", library}};
        const auto valid = QDir(directory).filePath("10-valid.json");
        const auto duplicate = QDir(directory).filePath("99-duplicate.json");
        write(valid, manifest);
        write(duplicate, manifest);
        const auto faulty = [&](const QString &name, const QString &field,
                                const QJsonValue &value) {
            auto m = manifest;
            m[field] = value;
            write(QDir(directory).filePath(name), m);
        };
        faulty("00-missing.json", "library", "not-installed.dll");
        faulty("01-kind.json", "kind", "unsupported");
        faulty("02-platform.json", "platform", "unsupported");
        faulty("03-path.json", "library", "../outside.dll");
        auto m = manifest;
        m["id"] = "org.sdr.test.device";
        m["library"] = bad;
        write(QDir(directory).filePath("04-abi.json"), m);
        m["library"] = unavailable;
        write(QDir(directory).filePath("05-runtime.json"), m);
        {
            QFile file(QDir(directory).filePath("06-broken.json"));
            check(file.open(QIODevice::WriteOnly), "Bad JSON fixture");
            file.write("{broken");
        }
        engine.post([&] { engine.scan(); });
        wait([&] { return engine.snapshot().devices.size() == 4; },
             "Unknown provider discovered without host changes");
        auto state = engine.snapshot();
        check(state.device_modules.size() == 1 && state.device_module_errors.size() == 8 &&
                  state.error.isEmpty(),
              "Faulty modules isolated from valid and software sources");
        const std::string id = "device:org.sdr.example.device:example:0";
        check(state.devices[2].gain && state.devices[2].ppm && state.devices[2].bias_t &&
                  state.devices[2].direct_sampling,
              "Provider capabilities available in application");
        config.sample_rate = 240001;
        config.center_hz = 100000001;
        config.gain = 16;
        config.ppm = 1;
        config.bias_t = true;
        config.direct_sampling = 2;
        engine.post([&] { engine.start(id, config); });
        wait([&] { return engine.snapshot().running && engine.snapshot().iq_samples > 0; },
             "Provider RX");
        state = engine.snapshot();
        check(state.sample_rate == 240000 && state.center_hz == 100000000 && state.gain == 16 &&
                  state.ppm == 1 && state.bias_t && state.direct_sampling == 2,
              "Provider actual parameters published");
        engine.post([&] { engine.scan(); });
        wait([&] { return engine.snapshot().error.contains("Stop RX before scanning"); },
             "Rescan while RX rejected");
        check(engine.snapshot().running, "Rejected rescan preserves live instance");
        engine.post([&] { engine.stop(); });
        wait([&] { return !engine.snapshot().running; }, "Provider stopped");
        check(QFile::rename(valid, valid + ".disabled") &&
                  QFile::rename(duplicate, duplicate + ".disabled"),
              "Remove provider manifests after stop");
        engine.post([&] { engine.scan(); });
        wait([&] { return engine.snapshot().devices.size() == 2; },
             "Removed module devices disappear");
        engine.post([&] { engine.start(id, config); });
        wait([&] { return engine.snapshot().error.contains("device module is unavailable"); },
             "Missing device is not replaced by another source");
        check(!engine.snapshot().running, "No automatic physical source substitution");
        check(QFile::rename(valid + ".disabled", valid), "Re-add provider manifest");
        engine.post([&] { engine.scan(); });
        wait([&] { return engine.snapshot().devices.size() == 4; }, "Re-added module visible");
        engine.post([&] { engine.start(id, config); });
        wait([&] { return engine.snapshot().running && engine.snapshot().error.isEmpty(); },
             "Re-added provider reopens");
        engine.post([&] { engine.stop(); });
        wait([&] { return !engine.snapshot().running; }, "Final stop");
        std::cout << "Device discovery, module isolation and zero-module RX passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
