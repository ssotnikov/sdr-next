#include "engine.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void wait(F condition, const char *message) {
    for (int i = 0; i < 2000; ++i) {
        if (condition())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("sdr-app-engine-tests");
    QStandardPaths::setTestModeEnabled(true);
    try {
        QTemporaryDir folder;
        check(folder.isValid(), "Temporary directory");
        AudioBuffer audio;
        audio.push(std::vector<float>(12001));
        check(audio.drops() == 1, "Audio overflow counted");
        audio.clear(false);
        check(audio.drops() == 1, "Stop preserves diagnostic counters");
        audio.clear();
        check(audio.drops() == 0, "New session resets diagnostic counters");
        Engine engine(audio);
        wait([&] { return engine.snapshot().devices.size() >= 2; }, "Source enumeration");
        sdr::Config config;
        config.sample_rate = 240000;
        config.mode = "AM";
        check(engine.post([&] { engine.start("synthetic", config); }), "Start command accepted");
        wait(
            [&] {
                const auto s = engine.snapshot();
                return s.running && s.iq_samples > 0;
            },
            "Synthetic receiver started");
        check(engine.post([&] { engine.record(folder.path(), true, true); }),
              "Record command accepted");
        wait([&] { return engine.snapshot().recording; }, "Recording started");
        const auto begin = engine.snapshot().iq_samples;
        wait([&] { return engine.snapshot().iq_samples > begin + 48000; },
             "Recording received samples");
        engine.post([&] { engine.start("synthetic", config); });
        wait([&] { return !engine.snapshot().error.isEmpty(); },
             "Reconfigure during recording rejected");
        check(engine.snapshot().running && engine.snapshot().recording,
              "Failed command preserves active capture");
        engine.post([&] { engine.stop_record(); });
        wait([&] { return !engine.snapshot().recording; }, "Recording stopped");
        engine.post([&] { engine.stop(); });
        wait([&] { return !engine.snapshot().running; }, "Receiver stopped");
        const auto recordings = QDir(folder.path()).entryList({"*.cf32"}, QDir::Files);
        check(recordings.size() == 1, "Single IQ recording");
        const auto raw = QDir(folder.path()).filePath(recordings.front());
        QFile metadata(raw + ".json");
        check(metadata.open(QIODevice::ReadOnly), "Metadata readable");
        const auto json = QJsonDocument::fromJson(metadata.readAll()).object();
        check(json["complete"].toBool(), "Metadata complete");
        check(json["samples"].toDouble() * 8 == QFileInfo(raw).size(), "Metadata sample count");
        check(QDir(folder.path()).entryList({"*.wav"}, QDir::Files).size() == 1,
              "WAV recording present");
        config.file = raw.toUtf8().toStdString();
        engine.post([&] { engine.start("file", config); });
        wait([&] { return engine.snapshot().running; }, "File source started");
        wait([&] { return !engine.snapshot().running; }, "File source reaches EOF");
        check(engine.snapshot().state == "Ready", "EOF releases receiver");
        // A paced source must resume at real-time speed after the worker was delayed.
        config.sample_rate = 2400000;
        engine.post([&] { engine.start("synthetic", config); });
        wait([&] { return engine.snapshot().frame >= 4; }, "Spectrum frames published");
        const auto before = engine.snapshot().iq_samples;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        check(engine.snapshot().running, "Paced source remains active");
        check(engine.snapshot().iq_samples - before >= 1900000,
              "DSP worker must not sleep between ready IQ blocks");
        engine.post([&] { engine.stop(); });
        wait([&] { return !engine.snapshot().running; }, "Paced source stopped");
        if (argc > 1) {
            const QString manifest = QString::fromLocal8Bit(argv[1]);
            engine.post([&] { engine.load_plugin(manifest); });
            wait([&] { return engine.snapshot().plugins.size() == 1; }, "Manifest installed");
            QFile original(manifest);
            check(original.open(QIODevice::ReadOnly), "Source manifest readable");
            const auto good = QJsonDocument::fromJson(original.readAll()).object();
            for (const QString field : {QString("kind"), QString("platform")}) {
                engine.post([&] { engine.start("synthetic", config); });
                wait([&] { return engine.snapshot().running && engine.snapshot().error.isEmpty(); },
                     "Clear earlier command error");
                engine.post([&] { engine.stop(); });
                wait([&] { return !engine.snapshot().running; }, "Stop before manifest validation");
                auto invalid = good;
                invalid[field] = "unsupported";
                const auto invalidPath = folder.filePath(field + ".json");
                {
                    QFile file(invalidPath);
                    check(file.open(QIODevice::WriteOnly), "Fixture writable");
                    file.write(QJsonDocument(invalid).toJson());
                }
                engine.post([&, invalidPath] { engine.load_plugin(invalidPath); });
                wait([&] { return engine.snapshot().error.contains("Incompatible"); },
                     "Invalid manifest rejected");
                check(engine.snapshot().plugins.size() == 1, "Invalid manifest is not registered");
            }
            engine.post([&] { engine.load_plugin(manifest); });
            wait([&] { return engine.snapshot().error.contains("Duplicate"); },
                 "Duplicate manifest rejected");
            if (argc > 2) {
                const auto binary = QString::fromLocal8Bit(argv[2]);
                const auto name = QFileInfo(binary).fileName();
                check(QFile::copy(binary, folder.filePath(name)), "Init-error library copied");
                auto invalid = good;
                invalid["id"] = "org.sdr.example.bad";
                invalid["library"] = name;
                const auto invalidPath = folder.filePath("init-error.json");
                {
                    QFile file(invalidPath);
                    check(file.open(QIODevice::WriteOnly), "Init fixture writable");
                    file.write(QJsonDocument(invalid).toJson());
                }
                engine.post([&, invalidPath] { engine.load_plugin(invalidPath); });
                wait([&] { return engine.snapshot().error.contains("Plugin rejected"); },
                     "Init failure reported");
                check(engine.snapshot().plugins.size() == 1,
                      "Init failure rolls back registration");
            }
            engine.post([&] { engine.start("plugin:org.sdr.example.source", config); });
            wait([&] { return engine.snapshot().running; }, "Plugin RX started");
            engine.post([&] { engine.unload_plugins(); });
            wait([&] { return engine.snapshot().error.contains("Stop RX before unloading"); },
                 "Busy unload rejected");
            check(engine.snapshot().running && engine.snapshot().plugins.size() == 1,
                  "Busy module retained");
            engine.post([&] { engine.stop(); });
            wait([&] { return !engine.snapshot().running; }, "Plugin RX stopped");
            engine.post([&] { engine.unload_plugins(); });
            wait([&] { return engine.snapshot().plugins.empty(); }, "Plugin disabled");
        }
        std::cout << "Engine integration tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
