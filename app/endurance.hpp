#pragma once
#include "controller.hpp"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QStorageInfo>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <psapi.h>
#endif

// Opt-in measurement driver. The normal receiver path is unchanged.
class Endurance final : public QObject {
    struct SpectrumCounters {
        std::atomic<quint64> published{0}, synced_sequence{0}, presented_sequence{0};
        std::atomic<quint64> synchronized{0}, presented{0}, swaps{0};
    };

    Controller &receiver_;
    QString directory_, prefix_, failure_;
    QFile metrics_;
    QFile audio_trace_;
    std::vector<AudioTrace::Event> trace_batch_, trace_previous_;
    bool trace_started_ = false, trace_followup_ = false;
    bool trace_truncated_ = false;
    qint64 trace_bytes_ = 0;
    QJsonObject trace_interval_;
    QTimer timer_;
    QElapsedTimer clock_, interval_;
    std::jthread probe_;
    std::vector<double> queue_delays_;
    double seconds_, cpu_start_ = 0, elapsed_ = 0, cpu_average_ = 0;
    double min_fps_ = 1e9, max_ram_ = 0;
    const std::shared_ptr<SpectrumCounters> spectrum_ = std::make_shared<SpectrumCounters>();
    quint64 shown_ = 0;
    bool requested_ = false, record_requested_ = false, stopping_ = false, complete_ = false;
    bool finished_ = false, ac_lost_ = false;
    QVariantMap last_status_;
    QString started_;

    static double cpu() {
#ifdef Q_OS_WIN
        FILETIME create{}, exit{}, kernel{}, user{};
        if (!GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user))
            return -1;
        const auto value = [](FILETIME t) {
            return (static_cast<quint64>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
        };
        return static_cast<double>(value(kernel) + value(user)) / 1e7;
#else
        return -1;
#endif
    }
    static QJsonObject memory() {
        QJsonObject result;
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS_EX m{};
        m.cb = sizeof(m);
        if (K32GetProcessMemoryInfo(GetCurrentProcess(),
                                    reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&m), sizeof(m))) {
            result["working_set_mib"] = static_cast<double>(m.WorkingSetSize) / 1048576;
            result["private_mib"] = static_cast<double>(m.PrivateUsage) / 1048576;
        }
        SYSTEM_POWER_STATUS power{};
        if (GetSystemPowerStatus(&power))
            result["ac_line_status"] = power.ACLineStatus;
#endif
        return result;
    }
    void append(QJsonObject row) {
        const auto bytes = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
        if (metrics_.write(bytes) != bytes.size() || !metrics_.flush()) {
            failure_ = "Cannot persist metrics";
            stop(false);
        }
    }
    bool writeTrace(const std::vector<AudioTrace::Event> &batch) {
        if (trace_truncated_ || batch.empty())
            return true;
        QByteArray bytes;
        bytes.reserve(static_cast<qsizetype>(batch.size() * 96));
        constexpr const char *names[] = {"iq_read", "iq_enqueue", "dsp", "push", "drop", "read",
                                         "prefill", "underrun", "clear", "record", "sink_state",
                                         "iq_dequeue"};
        for (const auto &event : batch) {
            bytes += QByteArray::number(static_cast<qlonglong>(event.time_us)) + ',' +
                     names[static_cast<unsigned>(event.kind)] + ',' +
                     QByteArray::number(static_cast<qulonglong>(event.block)) + ',' +
                     QByteArray::number(static_cast<qulonglong>(event.frames)) + ',' +
                     QByteArray::number(static_cast<qulonglong>(event.actual)) + ',' +
                     QByteArray::number(static_cast<qulonglong>(event.before)) + ',' +
                     QByteArray::number(static_cast<qulonglong>(event.after)) + ',' +
                     QByteArray::number(static_cast<qlonglong>(event.duration_us)) + ',' +
                     QByteArray::number(static_cast<qlonglong>(event.thread_cpu_us)) + ',' +
                     QByteArray::number(event.thread_id) + ',' +
                     QByteArray::number(static_cast<qlonglong>(event.driver_us)) + ',' +
                     QByteArray::number(static_cast<qlonglong>(event.conversion_us)) + ',' +
                     QByteArray::number(event.driver_result) + '\n';
        }
        constexpr qint64 max_trace_bytes = 128 * 1024 * 1024;
        if (bytes.size() > max_trace_bytes - trace_bytes_) {
            trace_truncated_ = true;
            return true;
        }
        trace_bytes_ += bytes.size();
        return audio_trace_.write(bytes) == bytes.size() && audio_trace_.flush();
    }
    bool collectTrace() {
        auto *trace = receiver_.audioTrace();
        if (!trace)
            return true;
        trace->take(trace_batch_);
        using Kind = AudioTrace::Kind;
        bool trigger = false;
        std::int64_t minimum = -1;
        std::int64_t max_iq_age = -1;
        std::size_t max_iq_depth = 0;
        for (const auto &event : trace_batch_) {
            if (event.kind == Kind::read || event.kind == Kind::underrun) {
                const auto after = static_cast<std::int64_t>(event.after);
                minimum = minimum < 0 ? after : std::min(minimum, after);
            }
            if (event.kind == Kind::iq_enqueue || event.kind == Kind::iq_dequeue)
                max_iq_depth = std::max({max_iq_depth, event.before, event.after});
            if (event.kind == Kind::iq_dequeue)
                max_iq_age = std::max(max_iq_age, event.duration_us);
            trigger |= event.kind == Kind::underrun || event.kind == Kind::drop ||
                       event.kind == Kind::clear || event.kind == Kind::sink_state ||
                       (event.kind == Kind::dsp && event.duration_us >= 20000) ||
                       (event.kind == Kind::iq_read &&
                        (event.duration_us >= 90000 ||
                         (event.driver_us >= 0 && event.driver_result < 0))) ||
                       (event.kind == Kind::iq_enqueue && event.actual < event.frames);
        }
        trace_interval_ = {{"events", static_cast<qint64>(trace_batch_.size())},
                           {"lost_total", static_cast<qint64>(trace->lost())},
                           {"min_after_read_frames", static_cast<qint64>(minimum)},
                           {"max_iq_depth_blocks", static_cast<qint64>(max_iq_depth)},
                           {"max_iq_age_us", static_cast<qint64>(max_iq_age)}};
        if (trigger || trace_followup_ || (!trace_started_ && !trace_batch_.empty())) {
            if (!writeTrace(trace_previous_) || !writeTrace(trace_batch_))
                return false;
            trace_previous_.clear();
            trace_started_ = true;
        } else {
            trace_previous_.swap(trace_batch_);
        }
        trace_followup_ = trigger;
        return true;
    }
    void stop(bool complete) {
        if (stopping_)
            return;
        stopping_ = true;
        complete_ = complete;
        receiver_.stopRecording();
        receiver_.stop();
        interval_.restart();
    }
    void summary(const QString &state) {
        auto delays = queue_delays_;
        std::sort(delays.begin(), delays.end());
        const auto p95 = delays.empty() ? -1 : delays[(delays.size() - 1) * 95 / 100];
        QJsonObject result{
            {"state", state},
            {"started_utc", started_},
            {"finished_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"requested_seconds", seconds_},
            {"elapsed_seconds", elapsed_},
            {"cpu_average_percent_total", cpu_average_},
            {"ram_max_mib", max_ram_},
            {"minimum_interval_presented_spectrum_fps", min_fps_ == 1e9 ? 0 : min_fps_},
            {"gui_queue_proxy_p95_ms", p95},
            {"gui_queue_proxy_count", static_cast<double>(delays.size())},
            {"ui_input_latency", "not_verified"},
            {"physical_audio_latency", "not_verified"},
            {"full_acceptance", "not_evaluated"},
            {"ac_lost", ac_lost_},
            {"error", failure_},
            {"last_streaming_status", QJsonObject::fromVariantMap(last_status_)}};
        if (auto *trace = receiver_.audioTrace()) {
            QJsonObject info{{"lost_total", static_cast<qint64>(trace->lost())},
                             {"truncated", trace_truncated_}};
#ifdef Q_OS_WIN
            LARGE_INTEGER before{}, after{}, frequency{};
            if (QueryPerformanceFrequency(&frequency) && QueryPerformanceCounter(&before)) {
                const auto offset = trace->offsetMicros(AudioTrace::Clock::now());
                if (QueryPerformanceCounter(&after))
                    info["qpc_anchor"] = QJsonObject{{"pid", QCoreApplication::applicationPid()},
                                                   {"trace_time_us", offset},
                                                   {"qpc_before", before.QuadPart},
                                                   {"qpc_after", after.QuadPart},
                                                   {"qpc_frequency", frequency.QuadPart}};
            }
#endif
            result["audio_trace"] = info;
        }
        QFile file(directory_ + "/summary.json");
        if (file.open(QIODevice::WriteOnly)) {
            const auto data = QJsonDocument(result).toJson();
            if (file.write(data) != data.size() || !file.flush())
                complete_ = false;
        } else
            complete_ = false;
    }
    void finish() {
        summary(complete_ ? "duration_completed" : "failed");
        finished_ = true;
        timer_.stop();
        probe_.request_stop();
#ifdef Q_OS_WIN
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
        if (complete_) {
            QCoreApplication::quit();
        } else {
            if (failure_.isEmpty())
                failure_ = "Endurance report could not be finalized";
            receiver_.reportTestFailure("Endurance FAILED: " + failure_);
            if (auto *window = qobject_cast<QQuickWindow *>(parent()))
                window->setTitle(QGuiApplication::applicationDisplayName() + " - Endurance FAILED");
        }
    }
    void tick() {
        auto status = receiver_.status();
        if (!collectTrace()) {
            failure_ = "Cannot persist audio trace";
            complete_ = false;
            stop(false);
        }
        if (stopping_) {
            if (!status["running"].toBool() && !status["recording"].toBool()) {
                finish();
            } else if (interval_.elapsed() > 15000) {
                failure_ = "Stop did not complete within 15 seconds";
                complete_ = false;
                finish();
            }
            return;
        }
        if (!status["error"].toString().isEmpty()) {
            failure_ = status["error"].toString();
            last_status_ = status;
            stop(false);
            return;
        }
        if (!requested_) {
            const auto devices = receiver_.devices();
            for (int i = 0; i < devices.size(); ++i) {
                if (!devices[i].toMap()["id"].toString().startsWith(prefix_))
                    continue;
                receiver_.setAudio(0, 0.02);
                if (auto *selector = parent()->findChild<QObject *>("sourceSelector"))
                    selector->setProperty("currentIndex", i);
                receiver_.start(i, {{"centerMHz", 100.0},
                                    {"rate", 2400000.0},
                                    {"offsetKHz", 0.0},
                                    {"mode", "WFM"},
                                    {"gain", 16.0},
                                    {"ppm", 0.0},
                                    {"biasT", false},
                                    {"directSampling", 0},
                                    {"squelch", -120.0}});
                requested_ = true;
                break;
            }
        }
        if (status["running"].toBool() && !record_requested_) {
            receiver_.record(directory_, true, false);
            record_requested_ = true;
        }
        if (!clock_.isValid()) {
            if (status["running"].toBool() && status["recording"].toBool() &&
                status["audioDeviceBufferFrames"].toLongLong() > 0) {
                started_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                cpu_start_ = cpu();
                clock_.start();
                interval_.restart();
                shown_ = spectrum_->presented.load(std::memory_order_relaxed);
            } else if (interval_.elapsed() > 15000) {
                failure_ = "Device, audio or recording did not start within 15 seconds";
                stop(false);
            }
            return;
        }
        elapsed_ = clock_.nsecsElapsed() / 1e9;
        const auto period = interval_.nsecsElapsed() / 1e9;
        interval_.restart();
        const auto presented = spectrum_->presented.load(std::memory_order_relaxed);
        const auto fps = static_cast<double>(presented - shown_) / period;
        shown_ = presented;
        min_fps_ = std::min(min_fps_, fps);
        auto row = memory();
        ac_lost_ |= row["ac_line_status"].toInt(-1) == 0;
        max_ram_ =
            std::max({max_ram_, row["working_set_mib"].toDouble(), row["private_mib"].toDouble()});
        cpu_average_ =
            100 * (cpu() - cpu_start_) / elapsed_ / std::max(1, QThread::idealThreadCount());
        row["utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        row["elapsed_seconds"] = elapsed_;
        row["cpu_average_percent_total"] = cpu_average_;
        row["presented_spectrum_fps"] = fps;
        const auto *window = qobject_cast<QQuickWindow *>(parent());
        row["window_visible"] = window && window->isVisible();
        row["window_exposed"] = window && window->isExposed();
        row["window_visibility"] = window ? static_cast<int>(window->visibility()) : -1;
        row["spectrum_interval_seconds"] = period;
        row["gui_spectra_published"] =
            static_cast<qint64>(spectrum_->published.load(std::memory_order_relaxed));
        row["gui_spectra_synchronized"] =
            static_cast<qint64>(spectrum_->synchronized.load(std::memory_order_relaxed));
        row["gui_spectra_presented"] = static_cast<qint64>(presented);
        row["window_frames_queued_for_presentation"] =
            static_cast<qint64>(spectrum_->swaps.load(std::memory_order_relaxed));
        row["gui_queue_proxy_last_ms"] = queue_delays_.empty() ? -1 : queue_delays_.back();
        row["status"] = QJsonObject::fromVariantMap(status);
        if (receiver_.audioTrace())
            row["audio_trace"] = trace_interval_;
        last_status_ = status;
        append(row);
        if (!status["running"].toBool())
            failure_ = "RX stopped before requested duration; state=" + status["state"].toString();
        else if (!status["recording"].toBool())
            failure_ = "Recording stopped before requested duration";
        else if (status["rate"].toDouble() != 2400000)
            failure_ = "Actual sample rate changed during test";
        else if (ac_lost_)
            failure_ = "AC power disconnected during test";
        if (!failure_.isEmpty()) {
            stop(false);
        } else if (elapsed_ >= seconds_)
            stop(true);
    }

  public:
    Endurance(Controller &receiver, QQuickWindow *window, const QString &directory, double seconds,
              const QString &prefix)
        : receiver_(receiver), directory_(directory), prefix_(prefix),
          metrics_(directory + "/metrics.jsonl"), seconds_(seconds) {
        setParent(window);
        if (!window || prefix.isEmpty() || !std::isfinite(seconds) || seconds < 1 ||
            seconds > 28800 || QFile::exists(directory) || !QDir().mkpath(directory))
            throw std::runtime_error(
                "Invalid endurance options or output directory already exists");
        const QStorageInfo storage(directory);
        if (storage.bytesAvailable() < static_cast<qint64>(seconds * 96000 + 1073741824) ||
            !metrics_.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            throw std::runtime_error("Insufficient disk space or cannot create metrics file");
        if (receiver_.audioTrace()) {
            audio_trace_.setFileName(directory + "/audio-trace.csv");
            constexpr char header[] =
                "time_us,kind,iq_sequence,frames,actual,queued_before,queued_after,duration_us,"
                "thread_cpu_us,thread_id,driver_us,conversion_us,driver_result\n";
            if (!audio_trace_.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                audio_trace_.write(header, sizeof(header) - 1) != sizeof(header) - 1)
                throw std::runtime_error("Cannot create audio trace");
        }
        window->setTitle(QGuiApplication::applicationDisplayName() +
                         " endurance - 2.4 MSPS / WFM / WAV");
#ifdef Q_OS_WIN
        if (!SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED))
            throw std::runtime_error("Cannot prevent automatic standby during test");
#endif
        connect(&receiver_, &Controller::spectrumReady, this, [counters = spectrum_] {
            counters->published.fetch_add(1, std::memory_order_relaxed);
        });
        connect(
            window, &QQuickWindow::beforeSynchronizing, this,
            [counters = spectrum_] {
                const auto sequence = counters->published.load(std::memory_order_relaxed);
                if (sequence !=
                    counters->synced_sequence.exchange(sequence, std::memory_order_relaxed))
                    counters->synchronized.fetch_add(1, std::memory_order_relaxed);
            },
            Qt::DirectConnection);
        connect(
            window, &QQuickWindow::frameSwapped, this,
            [counters = spectrum_] {
                const auto sequence = counters->synced_sequence.load(std::memory_order_relaxed);
                if (sequence !=
                    counters->presented_sequence.exchange(sequence, std::memory_order_relaxed))
                    counters->presented.fetch_add(1, std::memory_order_relaxed);
                counters->swaps.fetch_add(1, std::memory_order_relaxed);
            },
            Qt::DirectConnection);
        probe_ = std::jthread([this](std::stop_token token) {
            while (!token.stop_requested()) {
                const auto sent = std::chrono::steady_clock::now();
                QMetaObject::invokeMethod(
                    this,
                    [this, sent] {
                        if (clock_.isValid() && !stopping_)
                            queue_delays_.push_back(std::chrono::duration<double, std::milli>(
                                                        std::chrono::steady_clock::now() - sent)
                                                        .count());
                    },
                    Qt::QueuedConnection);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        });
        interval_.start();
        connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
        timer_.setTimerType(Qt::PreciseTimer);
        timer_.start(1000);
    }
    ~Endurance() override {
        probe_.request_stop();
        if (probe_.joinable())
            probe_.join();
        if (!finished_) {
            failure_ = "Process closed before normal completion";
            summary("interrupted");
        }
#ifdef Q_OS_WIN
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
    }
};
