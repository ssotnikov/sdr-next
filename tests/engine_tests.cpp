#include "engine.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
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
void check_audio_diagnostics() {
    AudioBuffer session(false, false);
    const auto previous = session.clear(true, true);
    session.push(std::vector<float>(4800, 0.25f));
    session.clear(false, true);
    const auto next = session.clear(true, true);
    session.push(std::vector<float>(4800, -0.25f));
    session.discard(previous);
    std::vector<float> frames(4800);
    session.read(reinterpret_cast<char *>(frames.data()), frames.size() * sizeof(float), previous);
    check(frames.front() == 0 && frames.back() == 0 && session.metrics().queued == frames.size() &&
              session.metrics().requested == 0 && session.underruns() == 0,
          "A retired audio device cannot read or discard new-session audio");
    session.read(reinterpret_cast<char *>(frames.data()), frames.size() * sizeof(float), next);
    check(frames.front() == -0.25f && frames.back() == -0.25f,
          "The active audio device receives only its own session's samples");

    AudioBuffer traced(true, false);
    std::vector<float> output(4801);
    const std::vector<float> block(4800, 0.25f);
    const auto read = [&](AudioBuffer &audio, std::size_t frames) {
        audio.read(reinterpret_cast<char *>(output.data()),
                   static_cast<qint64>(frames * sizeof(float)));
    };
    read(traced, 4800); // Startup silence before the first push.
    traced.push(block, 41);
    read(traced, 4800);
    check(output[0] == 0.25f, "Audio samples delivered before a producer pause");
    read(traced, 4800);
    check(output[0] == 0 && traced.underruns() == 1, "Producer pause causes post-start silence");
    traced.push(block, 42);
    traced.push(block, 43);
    traced.push(block, 44);
    check(traced.drops() == 1, "Catch-up burst can exceed the bounded queue");
    std::vector<AudioTrace::Event> events;
    traced.trace()->take(events);
    using Kind = AudioTrace::Kind;
    check(events.size() == 7 && events.front().kind == Kind::prefill &&
              events[1].block == 41 && events[3].kind == Kind::underrun &&
              events[3].actual == 0 && events[3].frames == 4800 &&
              events.back().kind == Kind::drop && events.back().before == 9600 &&
              events.back().after == 9600,
          "Trace distinguishes startup, starvation and rejected blocks");
    for (std::size_t i = 0; i <= AudioTrace::capacity; ++i)
        traced.trace()->add({Kind::clear});
    traced.trace()->take(events);
    check(events.size() == AudioTrace::capacity && traced.trace()->lost() == 1,
          "A stalled collector bounds memory and exposes trace loss");

    // 100 ms per iteration. One extra request every seven iterations is about 30 ppm.
    for (bool drift : {false, true}) {
        AudioBuffer audio(false, false);
        audio.push(std::vector<float>(500, 0.25f));
        for (int step = 1; step <= 3514; ++step) {
            audio.push(block);
            read(audio, drift && step % 7 == 0 ? 4801 : 4800);
        }
        check(audio.drops() == 0 && audio.underruns() == (drift ? 2 : 0),
              "Virtual-time audio consumption distinguishes matched clocks and sustained deficit");
    }
}
void check_playback_clock() {
    const std::vector<float> input(4800, 0.25f);
    std::vector<float> output(4801);
    for (const int skew : {-1, 1}) {
        AudioBuffer audio;
        audio.push(std::vector<float>(skew > 0 ? 500 : 6700, 0.25f));
        for (int step = 1; step <= 3514; ++step) {
            audio.push(input);
            const auto requested = 4800 + (step % 7 == 0 ? skew : 0);
            audio.read(reinterpret_cast<char *>(output.data()), requested * sizeof(float));
        }
        check(audio.underruns() == 0 && audio.drops() == 0,
              "Playback holds audio reserve with a 30 ppm mismatch in either direction");
        check(output.front() == 0.25f, "Clock compensation preserves a constant signal");
        const auto stats = audio.metrics();
        check(stats.queued >= 3300 && stats.queued <= 3900 &&
                  skew * stats.correction_ppm < 0 && std::abs(stats.correction_ppm) <= 1000,
              "Playback reserve converges with bounded correction in the expected direction");
        check(stats.accepted == stats.consumed + stats.queued &&
                  stats.requested == stats.delivered && stats.delivered != stats.consumed,
              "Interpolated output and source consumption are accounted separately");
        audio.read(reinterpret_cast<char *>(output.data()), 4800 * sizeof(float));
        check(audio.underruns() == 1 && output[4799] == 0,
              "A real producer pause still reports underrun and pads the missing output");
        audio.clear(false);
        check(audio.metrics().correction_ppm == 0 && audio.metrics().queued == 0,
              "Reset clears the playback controller and FIFO");
        audio.push(std::vector<float>(4800, -0.25f));
        audio.read(reinterpret_cast<char *>(output.data()), 4800 * sizeof(float));
        check(output.front() == -0.25f && output[4799] == -0.25f,
              "Restart does not interpolate across the old stream");
    }
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("sdr-app-engine-tests");
    QStandardPaths::setTestModeEnabled(true);
    try {
        check_playback_clock();
        check_audio_diagnostics();
        QTemporaryDir folder;
        check(folder.isValid(), "Temporary directory");
        AudioBuffer audio;
        audio.push(std::vector<float>(5000, 0.25f));
        char probe[400 * sizeof(float)];
        audio.read(probe, sizeof(probe));
        auto counts = audio.metrics();
        check(counts.input == 5000 && counts.accepted == 5000 && counts.requested == 400 &&
                  counts.delivered == 400 && counts.consumed == 400 &&
                  counts.queued == 4600 && counts.discarded == 0,
              "Audio frame accounting at actual push/read seam");
        audio.clear(false);
        counts = audio.metrics();
        check(counts.discarded == 4600 && counts.queued == 0 &&
                  counts.accepted == counts.consumed + counts.discarded,
              "Cleared audio is accounted without losing session metrics");
        audio.clear();
        check(audio.metrics().input == 0 && audio.metrics().discarded == 0,
              "New session resets audio accounting");
        audio.push(std::vector<float>(12001));
        check(audio.drops() == 1, "Audio overflow counted");
        audio.clear(false);
        check(audio.drops() == 1, "Stop preserves diagnostic counters");
        audio.clear();
        check(audio.drops() == 0, "New session resets diagnostic counters");
        {
            AudioBuffer idle_audio;
            Engine idle(idle_audio, folder.filePath("no-device-modules"));
            wait([&] { return idle.snapshot().devices.size() == 2; },
                 "Idle software sources ready");
            const auto before = idle.snapshot().idle_waits;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            check(idle.snapshot().idle_waits <= before + 2,
                  "Idle worker must not poll periodically without commands or IQ");
            std::atomic<bool> command_ran = false;
            check(idle.post([&] { command_ran = true; }), "Idle command accepted");
            wait([&] { return command_ran.load(); }, "Command wakes idle worker");
        }
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
            check(argc >= 3, "Both example manifests are required");
            const QString manifest = QString::fromLocal8Bit(argv[1]);
            const QString demodManifest = QString::fromLocal8Bit(argv[2]);
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
            for (int i = 3; i < argc; ++i) {
                const auto binary = QString::fromLocal8Bit(argv[i]);
                const auto name = QFileInfo(binary).fileName();
                check(QFile::copy(binary, folder.filePath(name)), "Init-error library copied");
                auto invalid = good;
                invalid["id"] = "org.sdr.example.bad";
                invalid["library"] = name;
                const auto invalidPath = folder.filePath(name + ".json");
                {
                    QFile file(invalidPath);
                    check(file.open(QIODevice::WriteOnly), "Init fixture writable");
                    file.write(QJsonDocument(invalid).toJson());
                }
                engine.post([&] { engine.load_plugin(manifest); });
                wait([&] { return engine.snapshot().error.contains("Duplicate"); },
                     "Reset previous validation error");
                engine.post([&, invalidPath] { engine.load_plugin(invalidPath); });
                wait([&] { return engine.snapshot().error.contains("Plugin rejected"); },
                     "Init failure reported");
                check(engine.snapshot().plugins.size() == 1,
                      "Init failure rolls back registration");
            }
            engine.post([&] { engine.load_plugin(demodManifest); });
            wait([&] { return engine.snapshot().plugins.size() == 2; }, "Demod manifest installed");
            engine.post([&] {
                engine.selected_demod = "org.sdr.example.demod";
                config.offset_hz = 0;
                engine.start("plugin:org.sdr.example.source", config);
            });
            wait([&] { return engine.snapshot().running && audio.metrics().queued >= 4800; },
                 "Source IQ passed through the demodulator to the audio buffer");
            std::vector<float> pluginAudio(256);
            audio.read(reinterpret_cast<char *>(pluginAudio.data()),
                       pluginAudio.size() * sizeof(float), engine.snapshot().audio_generation);
            check(std::any_of(pluginAudio.begin(), pluginAudio.end(),
                              [](float sample) { return std::abs(sample) > 0.01f; }),
                  "Demodulator produced nonzero audio");
            engine.post([&] { engine.unload_plugins(); });
            wait([&] { return engine.snapshot().error.contains("Stop RX before unloading"); },
                 "Busy unload rejected");
            check(engine.snapshot().running && engine.snapshot().plugins.size() == 2,
                  "Both busy modules retained");
            engine.post([&] { engine.stop(); });
            wait([&] { return !engine.snapshot().running; }, "Plugin RX stopped");
            engine.post([&] { engine.unload_plugins(); });
            wait([&] {
                const auto state = engine.snapshot();
                return state.plugins.empty() && !state.demods.contains("org.sdr.example.demod");
            }, "Both plugins disabled");
            engine.post([&] {
                engine.load_plugin(manifest);
                engine.load_plugin(demodManifest);
            });
            wait([&] { return engine.snapshot().plugins.size() == 2; }, "Both plugins reloaded");
            engine.post([&] { engine.unload_plugins(); });
            wait([&] { return engine.snapshot().plugins.empty(); }, "Reloaded plugins disabled");
            std::cout << "Plugin RX, audio, incompatible ABI rejection and disable/reload passed\n";
        }
        config.sample_rate = 2400000;
        AudioBuffer traced(true);
        Engine stalled_engine(traced, folder.filePath("no-device-modules"));
        stalled_engine.post([&] { stalled_engine.start("synthetic", config); });
        wait([&] {
            return stalled_engine.snapshot().running && stalled_engine.snapshot().iq_samples > 50000;
        },
             "RX active before deliberate worker stall");
        stalled_engine.post([] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); });
        wait([&] { return stalled_engine.snapshot().error.contains("IQ queue overflow"); },
             "Bounded IQ queue reports deliberate worker stall");
        const auto stalled = stalled_engine.snapshot();
        check(!stalled.running && stalled.overflows == 1 && stalled.max_enqueue_wait_ms >= 15,
              "Overflow and queue wait metrics survive fault teardown");
        check(stalled.max_worker_gap_ms >= 150,
              "Worker pause must remain visible when overflow is detected before next block");
        std::vector<AudioTrace::Event> events;
        traced.trace()->take(events);
        using Kind = AudioTrace::Kind;
        check(std::any_of(events.begin(), events.end(), [](const auto &event) {
                  return event.kind == Kind::iq_enqueue && event.frames == sdr::block_size &&
                         event.actual == 0 && event.before == 8 && event.after == 8;
              }),
              "Fault trace reports the full IQ queue at rejection");
        check(std::any_of(events.begin(), events.end(), [](const auto &event) {
                  return event.kind == Kind::iq_dequeue && event.actual == sdr::block_size &&
                         event.before == event.after + 1 && event.duration_us >= 0;
              }),
              "IQ dequeue traces the block age and queue depth");
        std::cout << "Engine integration tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
