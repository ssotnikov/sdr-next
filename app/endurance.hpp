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
#include <chrono>
#include <cmath>
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
    Controller &receiver_;
    QString directory_, prefix_, failure_;
    QFile metrics_;
    QTimer timer_;
    QElapsedTimer clock_, interval_;
    std::jthread probe_;
    std::vector<double> queue_delays_;
    double seconds_, cpu_start_ = 0, elapsed_ = 0, cpu_average_ = 0;
    double min_fps_ = 1e9, max_ram_ = 0;
    quint64 published_ = 0, presented_ = 0, last_presented_ = 0, shown_ = 0;
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
        QFile file(directory_ + "/summary.json");
        if (file.open(QIODevice::WriteOnly)) {
            const auto data = QJsonDocument(result).toJson();
            if (file.write(data) != data.size() || !file.flush())
                complete_ = false;
        } else
            complete_ = false;
    }
    void tick() {
        auto status = receiver_.status();
        if (stopping_) {
            if (!status["running"].toBool() && !status["recording"].toBool()) {
                summary(complete_ ? "duration_completed" : "failed");
                finished_ = true;
                QCoreApplication::exit(complete_ ? 0 : 2);
            } else if (interval_.elapsed() > 15000) {
                failure_ = "Stop did not complete within 15 seconds";
                complete_ = false;
                summary("failed");
                finished_ = true;
                QCoreApplication::exit(2);
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
                shown_ = presented_;
            } else if (interval_.elapsed() > 15000) {
                failure_ = "Device, audio or recording did not start within 15 seconds";
                stop(false);
            }
            return;
        }
        elapsed_ = clock_.nsecsElapsed() / 1e9;
        const auto period = interval_.nsecsElapsed() / 1e9;
        interval_.restart();
        const auto fps = static_cast<double>(presented_ - shown_) / period;
        shown_ = presented_;
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
        row["gui_queue_proxy_last_ms"] = queue_delays_.empty() ? -1 : queue_delays_.back();
        row["status"] = QJsonObject::fromVariantMap(status);
        last_status_ = status;
        append(row);
        if (!status["running"].toBool() || !status["recording"].toBool() ||
            status["rate"].toDouble() != 2400000 || ac_lost_) {
            failure_ = "RX/recording stopped, sample rate changed or AC disconnected";
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
        window->setTitle("SDR endurance - 2.4 MSPS / WFM / WAV");
#ifdef Q_OS_WIN
        if (!SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED))
            throw std::runtime_error("Cannot prevent automatic standby during test");
#endif
        connect(&receiver_, &Controller::spectrumReady, this, [this] { ++published_; });
        connect(
            window, &QQuickWindow::frameSwapped, this,
            [this] {
                if (published_ != last_presented_) {
                    ++presented_;
                    last_presented_ = published_;
                }
            },
            Qt::QueuedConnection);
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
