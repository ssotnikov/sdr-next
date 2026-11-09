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
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <time.h>
#endif

namespace {
std::uint32_t traceThreadId() {
#ifdef Q_OS_WIN
    return GetCurrentThreadId();
#else
    return 0;
#endif
}

std::int64_t threadCpuMicros() {
#ifdef Q_OS_WIN
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) {
        const auto ticks = [](FILETIME time) {
            return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        };
        return static_cast<std::int64_t>((ticks(kernel) + ticks(user)) / 10);
    }
#elif defined(Q_OS_LINUX)
    timespec time{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0)
        return static_cast<std::int64_t>(time.tv_sec) * 1000000 + time.tv_nsec / 1000;
#endif
    return -1;
}

double milliseconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}
void maximum(std::atomic<double> &value, double sample) {
    auto old = value.load(std::memory_order_relaxed);
    while (old < sample && !value.compare_exchange_weak(old, sample, std::memory_order_relaxed)) {
    }
}
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
void AudioBuffer::push(std::span<const float> samples, std::uint64_t block) {
    std::lock_guard lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    if (last_push_ != std::chrono::steady_clock::time_point{})
        metrics_.max_push_gap_ms =
            std::max(metrics_.max_push_gap_ms, milliseconds(now - last_push_));
    last_push_ = now;
    metrics_.input += samples.size();
    if (samples.size() > data_.size() - size_) {
        ++drops_;
        if (trace_)
            trace_->add({AudioTrace::Kind::drop, block, samples.size(), 0, size_, size_}, now);
        return;
    }
    metrics_.accepted += samples.size();
    for (float x : samples) {
        data_[(read_ + size_) % data_.size()] = x;
        ++size_;
    }
    if (trace_)
        trace_->add({AudioTrace::Kind::push, block, samples.size(), samples.size(),
                     size_ - samples.size(), size_}, now);
}
qint64 AudioBuffer::read(char *destination, qint64 bytes) {
    std::lock_guard lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    if (last_read_ != std::chrono::steady_clock::time_point{})
        metrics_.max_read_gap_ms =
            std::max(metrics_.max_read_gap_ms, milliseconds(now - last_read_));
    last_read_ = now;
    const auto count = static_cast<std::size_t>(bytes) / sizeof(float);
    if (size_ >= std::max<std::size_t>(4800, std::min(count, data_.size())))
        started_ = true;
    const auto before = size_;
    double step = 1;
    if (adaptive_ && started_) {
        constexpr double target_reserve = 3600;
        constexpr double gain_ppm_per_frame = 0.5;
        constexpr double limit_ppm = 1000;
        // Use requested frames as the time base, independent of callback jitter.
        const auto reserve = static_cast<double>(size_) - static_cast<double>(count);
        const auto weight = -std::expm1(-static_cast<double>(count) / (2.0 * sdr::audio_rate));
        filtered_reserve_ += weight * (reserve - filtered_reserve_);
        metrics_.correction_ppm = std::clamp((filtered_reserve_ - target_reserve) *
                                                 gain_ppm_per_frame, -limit_ppm, limit_ppm);
        step += metrics_.correction_ppm * 1e-6;
    }
    metrics_.requested += count;
    std::size_t available = 0;
    while (started_ && available < count && size_ > 0) {
        const auto current = data_[read_];
        auto value = current;
        if (phase_ != 0 && size_ > 1) {
            const auto previous = previous_sample_.value_or(current);
            const auto next = data_[(read_ + 1) % data_.size()];
            const auto following = size_ > 2 ? data_[(read_ + 2) % data_.size()] : next;
            // Catmull–Rom interpolation at a continuous fractional source position.
            value = static_cast<float>(
                current + 0.5 * phase_ *
                              (next - previous +
                               phase_ * (2 * previous - 5 * current + 4 * next - following +
                                         phase_ * (3 * (current - next) + following - previous))));
        }
        std::memcpy(destination + available * sizeof(float), &value, sizeof(float));
        ++available;
        phase_ += step;
        const auto advance = static_cast<std::size_t>(phase_);
        phase_ -= static_cast<double>(advance);
        const auto consumed = std::min(advance, size_);
        if (consumed != 0) {
            previous_sample_ = data_[(read_ + consumed - 1) % data_.size()];
            read_ = (read_ + consumed) % data_.size();
            size_ -= consumed;
        }
        if (advance > consumed)
            phase_ = 0;
    }
    metrics_.delivered += available;
    metrics_.consumed += before - size_;
    std::memset(destination + available * sizeof(float), 0,
                static_cast<std::size_t>(bytes) - available * sizeof(float));
    if (started_ && available < count) {
        ++underruns_;
        phase_ = 0;
        previous_sample_.reset();
    }
    if (trace_) {
        const auto kind = !started_ ? AudioTrace::Kind::prefill
                         : available < count ? AudioTrace::Kind::underrun : AudioTrace::Kind::read;
        trace_->add({kind, 0, count, available, before, size_}, now);
    }
    return bytes;
}
void AudioBuffer::clear(bool reset_metrics) {
    std::lock_guard lock(mutex_);
    if (trace_)
        trace_->add({AudioTrace::Kind::clear, 0, 0, 0, size_, 0});
    if (!reset_metrics)
        metrics_.discarded += size_;
    read_ = size_ = 0;
    started_ = false;
    filtered_reserve_ = 3600;
    phase_ = 0;
    previous_sample_.reset();
    metrics_.correction_ppm = 0;
    if (reset_metrics) {
        underruns_ = drops_ = 0;
        metrics_ = {};
        last_read_ = last_push_ = {};
    }
}
AudioBuffer::Metrics AudioBuffer::metrics() {
    std::lock_guard lock(mutex_);
    auto value = metrics_;
    value.queued = size_;
    return value;
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
    if (!commands_.push(std::move(command)))
        return false;
    notify_work();
    return true;
}
void Engine::notify_work() {
    {
        std::lock_guard lock(wake_mutex_);
        ++work_revision_;
    }
    wake_.notify_one();
}
Snapshot Engine::snapshot() {
    std::lock_guard lock(snapshot_mutex_);
    return snapshot_;
}
void Engine::publish() {
    const auto began = std::chrono::steady_clock::now();
    std::lock_guard lock(snapshot_mutex_);
    current_.max_source_read_ms = max_source_read_ms_;
    current_.max_enqueue_wait_ms = max_enqueue_wait_ms_;
    snapshot_ = current_;
    current_.max_publish_ms =
        std::max(current_.max_publish_ms, milliseconds(std::chrono::steady_clock::now() - began));
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
    max_source_read_ms_ = max_enqueue_wait_ms_ = 0;
    current_.max_source_read_ms = current_.max_enqueue_wait_ms = current_.max_dsp_ms = 0;
    current_.max_record_write_ms = current_.max_worker_gap_ms = current_.max_publish_ms = 0;
    current_.max_spectrum_ms = current_.max_audio_push_ms = current_.max_block_ms = 0;
    last_block_time_ = {};
    next_spectrum_time_ = {};
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
            sdr::DeviceSource *device_source = nullptr;
            if (device_module) {
                auto device = std::make_unique<sdr::DeviceSource>(device_module, device_key, actual);
                device_source = device.get();
                source = std::move(device);
            } else if (module)
                source = std::make_unique<PluginSource>(module, actual);
            else
                source = sdr::open_source(id, actual);
            promise->set_value(actual);
            announced = true;
            auto deadline = std::chrono::steady_clock::now();
            auto last_data = deadline;
            auto *trace = audio_.trace();
            const auto thread_id = trace ? traceThreadId() : 0;
            while (!token.stop_requested()) {
                auto block = std::make_shared<sdr::Block>();
                sdr::DeviceReadTrace read_trace;
                const auto read_began = std::chrono::steady_clock::now();
                const auto log_read = [&](AudioTrace::Clock::time_point end) {
                    if (trace)
                        trace->add({AudioTrace::Kind::iq_read, block->sequence, block->samples.size(),
                                    0, 0, 0, 0, AudioTrace::micros(end - read_began), -1, thread_id,
                                    read_trace.driver.driver_us, read_trace.conversion_us,
                                    read_trace.driver.driver_result},
                                   end);
                };
                bool has_data;
                try {
                    has_data = trace && device_source ? device_source->read(*block, &read_trace)
                                                      : source->read(*block);
                } catch (...) {
                    log_read(AudioTrace::Clock::now());
                    throw;
                }
                const auto read_finished = std::chrono::steady_clock::now();
                maximum(max_source_read_ms_, milliseconds(read_finished - read_began));
                const auto sequence = block->sequence;
                log_read(read_finished);
                if (!has_data)
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
                const auto enqueue_began = std::chrono::steady_clock::now();
                std::size_t depth = 0;
                const bool enqueued = iq_.push_for({std::move(block), read_finished},
                                                   std::chrono::milliseconds(20), token,
                                                   trace ? &depth : nullptr);
                const auto enqueue_finished = std::chrono::steady_clock::now();
                maximum(max_enqueue_wait_ms_, milliseconds(enqueue_finished - enqueue_began));
                if (trace)
                    trace->add({AudioTrace::Kind::iq_enqueue, sequence, count, enqueued ? count : 0,
                                depth - static_cast<std::size_t>(enqueued), depth, 0,
                                AudioTrace::micros(enqueue_finished - enqueue_began), -1, thread_id},
                               enqueue_finished);
                if (!enqueued) {
                    if (token.stop_requested())
                        break;
                    ++overflows_;
                    throw std::runtime_error(
                        "IQ queue overflow: processing/storage cannot keep up; RX stopped");
                }
                notify_work();
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
        notify_work();
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
        const auto observed_revision = work_revision_.load();
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
                // Preserve a worker stall even if acquisition faults before another block is
                // popped.
                if (last_block_time_ != std::chrono::steady_clock::time_point{})
                    current_.max_worker_gap_ms =
                        std::max(current_.max_worker_gap_ms,
                                 milliseconds(std::chrono::steady_clock::now() - last_block_time_));
                QString failure;
                {
                    std::lock_guard lock(failure_mutex_);
                    failure = acquisition_failure_;
                }
                if (!failure.isEmpty())
                    throw std::runtime_error(failure.toStdString());
                auto *trace = audio_.trace();
                std::size_t depth = 0;
                if (auto pending = iq_.pop(trace ? &depth : nullptr)) {
                    worked = true;
                    const auto began = std::chrono::steady_clock::now();
                    if (last_block_time_ != std::chrono::steady_clock::time_point{})
                        current_.max_worker_gap_ms = std::max(
                            current_.max_worker_gap_ms, milliseconds(began - last_block_time_));
                    last_block_time_ = began;
                    const auto &b = *pending->block;
                    const auto thread_id = trace ? traceThreadId() : 0;
                    if (trace)
                        trace->add({AudioTrace::Kind::iq_dequeue, b.sequence, b.samples.size(),
                                    b.samples.size(), depth + 1, depth, 0,
                                    AudioTrace::micros(began - pending->read_finished), -1,
                                    thread_id},
                                   began);
                    if (b.discontinuity) {
                        demod_.configure(config);
                        if (plugin_demod_)
                            throw std::runtime_error("Plugin processing stopped after IQ "
                                                     "discontinuity; reset requires restart");
                    }
                    const auto cpu_began = trace ? threadCpuMicros() : -1;
                    const auto dsp_began = std::chrono::steady_clock::now();
                    demod_.process(b.samples, audio_block_);
                    if (plugin_demod_)
                        plugin_demod_->process(b.samples, audio_block_);
                    const auto dsp_finished = std::chrono::steady_clock::now();
                    current_.max_dsp_ms =
                        std::max(current_.max_dsp_ms,
                                 milliseconds(dsp_finished - dsp_began));
                    if (trace) {
                        const auto cpu_finished = threadCpuMicros();
                        trace->add({AudioTrace::Kind::dsp, b.sequence, b.samples.size(),
                                    audio_block_.size(), 0, 0, 0,
                                    AudioTrace::micros(dsp_finished - dsp_began),
                                    cpu_began >= 0 && cpu_finished >= cpu_began
                                        ? cpu_finished - cpu_began : -1, thread_id}, dsp_finished);
                    }
                    const auto push_began = std::chrono::steady_clock::now();
                    audio_.push(audio_block_, b.sequence);
                    current_.max_audio_push_ms =
                        std::max(current_.max_audio_push_ms,
                                 milliseconds(std::chrono::steady_clock::now() - push_began));
                    if (current_.recording) {
                        const auto record_began = std::chrono::steady_clock::now();
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
                        const auto record_finished = std::chrono::steady_clock::now();
                        if (trace)
                            trace->add({AudioTrace::Kind::record, b.sequence, audio_block_.size(),
                                        0, 0, 0, 0, AudioTrace::micros(record_finished - record_began)},
                                       record_finished);
                        current_.max_record_write_ms =
                            std::max(current_.max_record_write_ms,
                                     milliseconds(record_finished - record_began));
                    }
                    current_.iq_samples += b.samples.size();
                    current_.power_db = demod_.power_db();
                    if (began >= next_spectrum_time_) {
                        constexpr auto period = std::chrono::milliseconds(50);
                        if (next_spectrum_time_ == std::chrono::steady_clock::time_point{})
                            next_spectrum_time_ = began;
                        next_spectrum_time_ += period * (1 + (began - next_spectrum_time_) / period);
                        const auto spectrum_began = std::chrono::steady_clock::now();
                        current_.spectrum = sdr::spectrum(b.samples);
                        current_.max_spectrum_ms = std::max(
                            current_.max_spectrum_ms,
                            milliseconds(std::chrono::steady_clock::now() - spectrum_began));
                        ++current_.frame;
                    }
                    current_.processing_ms = std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - began)
                                                 .count();
                    current_.max_block_ms = std::max(current_.max_block_ms, current_.processing_ms);
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
        if (!worked)
            ++current_.idle_waits;
        publish();
        if (!worked) {
            std::unique_lock lock(wake_mutex_);
            wake_.wait(lock, token, [this, observed_revision] {
                return work_revision_.load() != observed_revision;
            });
        }
    }
    try {
        stop();
    } catch (...) {
    }
}
