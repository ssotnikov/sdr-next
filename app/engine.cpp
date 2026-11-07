#include "engine.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <future>
#include <sdr/path.hpp>

namespace {
std::filesystem::path path(const QString &value) {
    return sdr::path_from_utf8(value.toUtf8().toStdString());
}
class PluginSource final : public sdr::Source {
    sdr::PluginInstance instance_;

  public:
    PluginSource(std::shared_ptr<sdr::PluginModule> module, const sdr::Config &c)
        : instance_(std::move(module), c) {}
    bool read(sdr::Block &block) override { return instance_.read(block); }
};
} // namespace
void AudioBuffer::push(std::span<const float> samples) {
    std::lock_guard lock(mutex_);
    if (samples.size() > data_.size() - size_) {
        ++drops_;
        return;
    }
    for (float x : samples) {
        data_[(read_ + size_) % data_.size()] = x;
        ++size_;
    }
}
qint64 AudioBuffer::read(char *destination, qint64 bytes) {
    std::lock_guard lock(mutex_);
    const auto count = static_cast<std::size_t>(bytes) / sizeof(float);
    if (size_ >= std::max<std::size_t>(4800, std::min(count, data_.size())))
        started_ = true;
    std::size_t available = started_ ? std::min(count, size_) : 0;
    for (std::size_t i = 0; i < available; ++i) {
        const float v = data_[read_];
        std::memcpy(destination + i * sizeof(float), &v, sizeof(float));
        read_ = (read_ + 1) % data_.size();
        --size_;
    }
    std::memset(destination + available * sizeof(float), 0,
                static_cast<std::size_t>(bytes) - available * sizeof(float));
    if (started_ && available < count)
        ++underruns_;
    return bytes;
}
void AudioBuffer::clear(bool reset_metrics) {
    std::lock_guard lock(mutex_);
    read_ = size_ = 0;
    started_ = false;
    if (reset_metrics)
        underruns_ = drops_ = 0;
}
Engine::Engine(AudioBuffer &audio, const QString &device_directory)
    : audio_(audio), device_directory_(device_directory.isEmpty()
                                           ? QCoreApplication::applicationDirPath() + "/devices"
                                           : device_directory) {
    worker_ = std::jthread([this](std::stop_token token) { run(token); });
    post([this] { scan(); });
}
Engine::~Engine() {
    worker_.request_stop();
    if (worker_.joinable())
        worker_.join();
}
bool Engine::post(std::function<void()> command) {
    return commands_.push(std::move(command));
}
Snapshot Engine::snapshot() {
    std::lock_guard lock(snapshot_mutex_);
    return snapshot_;
}
void Engine::publish() {
    std::lock_guard lock(snapshot_mutex_);
    snapshot_ = current_;
}
void Engine::event(const QString &code, const QString &message) {
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    QFile file(directory + "/diagnostics.jsonl");
    if (file.size() > 4 * 1024 * 1024) {
        file.remove();
    }
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        const QJsonObject object{
            {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"session", session_id_},
            {"code", code},
            {"message", message}};
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + "\n");
    }
}
void Engine::scan() {
    if (current_.running)
        throw std::runtime_error("Stop RX before scanning devices");
    current_.devices = sdr::enumerate_devices();
    device_registry_.clear();
    device_modules_.clear();
    current_.device_modules.clear();
    current_.device_module_errors.clear();
    const QDir directory(device_directory_);
    for (const auto &name : directory.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        try {
            QFile file(directory.filePath(name));
            if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
                throw std::runtime_error("Cannot read device manifest");
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject())
                throw std::runtime_error("Invalid device manifest JSON");
            const auto m = doc.object();
            const auto id = m["id"].toString();
            const auto library = m["library"].toString();
            const QString platform =
#ifdef Q_OS_WIN
                "Windows";
#else
                "Linux";
#endif
            if (id.isEmpty() || id.size() > 128 || id.contains(':') ||
                m["version"].toString().isEmpty() || m["license"].toString().isEmpty() ||
                m["kind"].toString() != "device" || m["platform"].toString() != platform ||
                m["arch"].toString() != QSysInfo::currentCpuArchitecture() ||
                m["api_major"].toInt(-1) != SDR_DEVICE_ABI_MAJOR || m["api_minor"].toInt(-1) < 0 ||
                m["api_minor"].toInt() > SDR_DEVICE_ABI_MINOR || library.isEmpty() ||
                library == "." || library == ".." || library.contains('/') ||
                library.contains('\\') || library.contains(':') ||
                QFileInfo(library).fileName() != library)
                throw std::runtime_error("Incompatible device manifest or library path");
            if (current_.device_modules.contains(id))
                throw std::runtime_error("Duplicate device provider ID");
            auto module = std::make_shared<sdr::DeviceModule>(path(directory.filePath(library)),
                                                              id.toStdString());
            auto devices = module->devices();
            for (auto &device : devices) {
                const auto key = device.id;
                device.id = "device:" + id.toStdString() + ":" + key;
                if (device_registry_.contains(device.id))
                    throw std::runtime_error("Duplicate device ID");
                device_registry_.emplace(device.id, DeviceBinding{module, key});
                current_.devices.push_back(std::move(device));
            }
            device_modules_.push_back(std::move(module));
            current_.device_modules.push_back(id);
            event("device.module.loaded", id);
        } catch (const std::exception &e) {
            const auto message = name + ": " + QString::fromUtf8(e.what());
            current_.device_module_errors.push_back(message);
            event("device.module.unavailable", message);
        }
    }
    for (const auto &m : modules_)
        if (m->api().kind == SDR_SOURCE)
            current_.devices.push_back({std::string("plugin:") + m->api().id, m->api().id, {}});
    event("source.scan", QString::number(current_.devices.size()));
}
void Engine::start(const std::string &id, sdr::Config requested) {
    if (current_.recording)
        throw std::runtime_error("Stop recording before changing the receiver configuration");
    stop();
    current_.state = "Opening";
    current_.error.clear();
    publish();
    session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    selected = id;
    config = requested;
    if (id == "file") {
        const QString sidecar = QString::fromStdString(config.file) + ".json";
        QFile file(sidecar);
        if (file.exists()) {
            if (!file.open(QIODevice::ReadOnly))
                throw std::runtime_error("Cannot read IQ metadata");
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject())
                throw std::runtime_error("Invalid IQ metadata JSON");
            const auto meta = doc.object();
            if (meta["version"].toInt() != 1 || meta["datatype"].toString() != "cf32_le" ||
                meta["sample_rate"].toDouble() < 48000)
                throw std::runtime_error("Unsupported IQ metadata version, format or rate");
            if (!meta["complete"].toBool())
                throw std::runtime_error(
                    "Incomplete recording: recovery must be explicitly reviewed before playback");
            if (!meta["discontinuities"].toArray().isEmpty())
                throw std::runtime_error(
                    "Recording contains IQ gaps: continuous playback would hide them");
            config.sample_rate = meta["sample_rate"].toDouble();
            config.center_hz = meta["center_hz"].toDouble();
        }
    }
    auto promise = std::make_shared<std::promise<sdr::Config>>();
    auto ready = promise->get_future();
    std::shared_ptr<sdr::PluginModule> module;
    std::shared_ptr<sdr::DeviceModule> device_module;
    std::string device_key;
    if (id.starts_with("device:")) {
        const auto found = device_registry_.find(id);
        if (found == device_registry_.end())
            throw std::runtime_error("Selected device module is unavailable");
        device_module = found->second.module;
        device_key = found->second.key;
    }
    if (id.starts_with("plugin:")) {
        for (const auto &m : modules_)
            if (id == std::string("plugin:") + m->api().id)
                module = m;
        if (!module)
            throw std::runtime_error("Selected source plugin is unavailable");
    }
    acquisition_done_ = false;
    gaps_ = 0;
    overflows_ = 0;
    {
        std::lock_guard lock(failure_mutex_);
        acquisition_failure_.clear();
    }
    const auto initial = config;
    acquisition_ = std::jthread([this, promise, initial, id, module, device_module,
                                 device_key](std::stop_token token) {
        bool announced = false;
        try {
            auto actual = initial;
            std::unique_ptr<sdr::Source> source;
            if (device_module)
                source = std::make_unique<sdr::DeviceSource>(device_module, device_key, actual);
            else if (module)
                source = std::make_unique<PluginSource>(module, actual);
            else
                source = sdr::open_source(id, actual);
            promise->set_value(actual);
            announced = true;
            auto deadline = std::chrono::steady_clock::now();
            auto last_data = deadline;
            while (!token.stop_requested()) {
                auto block = std::make_shared<sdr::Block>();
                if (!source->read(*block))
                    break;
                if (block->samples.empty()) {
                    if (std::chrono::steady_clock::now() - last_data >= std::chrono::seconds(5))
                        throw std::runtime_error("Source timeout: no IQ for 5 seconds");
                    continue;
                }
                last_data = std::chrono::steady_clock::now();
                if (block->discontinuity)
                    ++gaps_;
                const auto count = block->samples.size();
                if (!iq_.push_for(std::move(block), std::chrono::milliseconds(20), token)) {
                    if (token.stop_requested())
                        break;
                    ++overflows_;
                    throw std::runtime_error(
                        "IQ queue overflow: processing/storage cannot keep up; RX stopped");
                }
                if (source->paced()) {
                    // Allow ordinary timer jitter, but cap catch-up after a long scheduler stall.
                    deadline = std::max(deadline, std::chrono::steady_clock::now() -
                                                      std::chrono::milliseconds(20));
                    deadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(count / actual.sample_rate));
                    while (!token.stop_requested() && std::chrono::steady_clock::now() < deadline)
                        std::this_thread::sleep_for(
                            std::min(std::chrono::milliseconds(2),
                                     std::chrono::duration_cast<std::chrono::milliseconds>(
                                         deadline - std::chrono::steady_clock::now())));
                }
            }
        } catch (...) {
            if (!announced)
                promise->set_exception(std::current_exception());
            else {
                try {
                    throw;
                } catch (const std::exception &e) {
                    std::lock_guard lock(failure_mutex_);
                    acquisition_failure_ = QString::fromUtf8(e.what());
                }
            }
        }
        acquisition_done_ = true;
    });
    try {
        config = ready.get();
        demod_.configure(config);
        plugin_demod_.reset();
        if (!selected_demod.empty()) {
            for (const auto &m : modules_)
                if (selected_demod == m->api().id && m->api().kind == SDR_DEMODULATOR)
                    plugin_demod_ = std::make_unique<sdr::PluginInstance>(m, config);
            if (!plugin_demod_)
                throw std::runtime_error("Selected demodulator plugin is unavailable");
        }
    } catch (...) {
        acquisition_.request_stop();
        if (acquisition_.joinable())
            acquisition_.join();
        iq_.clear();
        throw;
    }
    audio_.clear();
    current_.iq_samples = 0;
    current_.gaps = 0;
    current_.overflows = 0;
    current_.sample_rate = config.sample_rate;
    current_.center_hz = config.center_hz;
    current_.gain = config.gain;
    current_.ppm = config.ppm;
    current_.bias_t = config.bias_t;
    current_.direct_sampling = config.direct_sampling;
    current_.running = true;
    current_.state = "Streaming";
    event("rx.start", QString::fromStdString(id));
}
void Engine::stop() {
    if (current_.running) {
        current_.state = "Stopping";
        publish();
    }
    acquisition_.request_stop();
    if (acquisition_.joinable())
        acquisition_.join();
    iq_.clear();
    plugin_demod_.reset();
    audio_.clear(false);
    stop_record();
    current_.running = false;
    current_.queue_depth = 0;
    current_.gaps = gaps_;
    current_.overflows = overflows_;
    current_.state = "Ready";
    event("rx.stop", {});
}
void Engine::metadata(bool complete) {
    if (!raw_on_ || metadata_path_.isEmpty())
        return;
    metadata_["complete"] = complete;
    metadata_["samples"] = static_cast<qint64>(raw_.samples());
    metadata_["discontinuities"] = recording_gaps_;
    QSaveFile file(metadata_path_);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(metadata_).toJson()) < 0 ||
        !file.commit())
        throw std::runtime_error("IQ metadata could not be committed");
}
void Engine::record(const QString &directory, bool wav, bool raw) {
    if (!current_.running || current_.recording || (!wav && !raw))
        throw std::runtime_error("Recording needs active RX and at least one format");
    if (!QDir(directory).exists())
        throw std::runtime_error("Recording folder does not exist");
    const QString stem = QDir(directory).filePath(
        "sdr-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + "-" +
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    wav_on_ = wav;
    raw_on_ = raw;
    recording_gaps_ = {};
    metadata_path_ = stem + ".cf32.json";
    try {
        if (wav)
            wav_.open(path(stem + ".wav"));
        if (raw)
            raw_.open(path(stem + ".cf32"));
        metadata_ = {{"version", 1},
                     {"datatype", "cf32_le"},
                     {"sample_rate", config.sample_rate},
                     {"center_hz", config.center_hz},
                     {"started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                     {"time_domain", "file_sample_index"},
                     {"complete", false},
                     {"capture_point", "normalized_source_before_ddc"}};
        metadata(false);
        current_.recording = true;
        event("record.start", stem);
    } catch (...) {
        try {
            wav_.close();
            raw_.close();
        } catch (...) {
        }
        wav_on_ = raw_on_ = false;
        throw;
    }
}
void Engine::stop_record(bool complete) {
    if (!wav_on_ && !raw_on_)
        return;
    QString error;
    try {
        wav_.close();
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
        complete = false;
    }
    try {
        raw_.close();
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
        complete = false;
    }
    try {
        metadata(complete);
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
    }
    wav_on_ = raw_on_ = false;
    current_.recording = false;
    if (!error.isEmpty()) {
        current_.error = error;
        event("record.incomplete", error);
    } else
        event("record.stop", complete ? "complete" : "incomplete");
}
void Engine::load_plugin(const QString &manifest) {
    if (current_.running)
        throw std::runtime_error("Stop RX before installing a trusted plugin");
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
        throw std::runtime_error("Cannot read plugin manifest");
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("Invalid plugin manifest JSON");
    const auto m = document.object();
    const auto id = m["id"].toString();
    const auto kind = m["kind"].toString();
    const auto library = m["library"].toString();
    const QString platform =
#ifdef Q_OS_WIN
        "Windows";
#else
        "Linux";
#endif
    if (id.isEmpty() || m["version"].toString().isEmpty() || m["license"].toString().isEmpty() ||
        m["platform"].toString() != platform ||
        m["arch"].toString() != QSysInfo::currentCpuArchitecture() ||
        m["api_major"].toInt() != SDR_ABI_MAJOR || m["api_minor"].toInt(-1) < 0 ||
        m["api_minor"].toInt() > SDR_ABI_MINOR || (kind != "source" && kind != "demodulator") ||
        library.isEmpty() || QFileInfo(library).fileName() != library || library == "." ||
        library == "..")
        throw std::runtime_error("Incompatible plugin manifest or library path");
    for (const auto &module : modules_)
        if (id == QString::fromUtf8(module->api().id))
            throw std::runtime_error("Duplicate plugin identity");
    const auto full = QFileInfo(manifest).absoluteDir().filePath(library);
    auto module = std::make_shared<sdr::PluginModule>(
        path(full), id.toStdString(), kind == "source" ? SDR_SOURCE : SDR_DEMODULATOR);
    modules_.push_back(module);
    current_.plugins.push_back(id);
    if (kind == "demodulator")
        current_.demods.push_back(id);
    scan();
    event("plugin.load", id);
}
void Engine::unload_plugins() {
    if (current_.running)
        throw std::runtime_error("Stop RX before unloading source/demodulator plugins");
    plugin_demod_.reset();
    modules_.clear();
    current_.plugins.clear();
    current_.demods = {"AM", "NFM", "WFM"};
    selected_demod.clear();
    scan();
    event("plugin.unload", {});
}
void Engine::run(std::stop_token token) {
    while (!token.stop_requested()) {
        bool worked = false;
        try {
            while (auto command = commands_.pop()) {
                worked = true;
                try {
                    (*command)();
                } catch (const std::exception &e) {
                    current_.error = QString::fromUtf8(e.what());
                    event("command.error", current_.error);
                    if (!current_.running)
                        current_.state = "Faulted";
                }
            }
            if (current_.running) {
                QString failure;
                {
                    std::lock_guard lock(failure_mutex_);
                    failure = acquisition_failure_;
                }
                if (!failure.isEmpty())
                    throw std::runtime_error(failure.toStdString());
                if (auto block = iq_.pop()) {
                    worked = true;
                    const auto began = std::chrono::steady_clock::now();
                    const auto &b = **block;
                    if (b.discontinuity) {
                        demod_.configure(config);
                        if (plugin_demod_)
                            throw std::runtime_error("Plugin processing stopped after IQ "
                                                     "discontinuity; reset requires restart");
                    }
                    demod_.process(b.samples, audio_block_);
                    if (plugin_demod_)
                        plugin_demod_->process(b.samples, audio_block_);
                    audio_.push(audio_block_);
                    if (current_.recording) {
                        try {
                            if (raw_on_) {
                                if (b.discontinuity)
                                    recording_gaps_.append(static_cast<qint64>(raw_.samples()));
                                raw_.write(b.samples);
                            }
                            if (wav_on_)
                                wav_.write(audio_block_);
                        } catch (const std::exception &e) {
                            stop_record(false);
                            current_.error = QString::fromUtf8(e.what());
                            event("record.error", current_.error);
                        }
                    }
                    current_.iq_samples += b.samples.size();
                    current_.power_db = demod_.power_db();
                    if (began - frame_time_ >= std::chrono::milliseconds(50)) {
                        current_.spectrum = sdr::spectrum(b.samples);
                        ++current_.frame;
                        frame_time_ = began;
                    }
                    current_.processing_ms = std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - began)
                                                 .count();
                } else if (acquisition_done_) {
                    stop();
                    current_.state = "Ready";
                }
                current_.queue_depth = iq_.size();
                current_.gaps = gaps_;
                current_.overflows = overflows_;
            }
        } catch (const std::exception &e) {
            const auto message = QString::fromUtf8(e.what());
            try {
                stop_record(false);
                stop();
            } catch (...) {
            }
            current_.state = "Faulted";
            current_.running = false;
            current_.error = message;
            event("error", message);
        }
        publish();
        if (!worked)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    try {
        stop();
    } catch (...) {
    }
}
