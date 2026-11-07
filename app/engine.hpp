#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <atomic>
#include <functional>
#include <mutex>
#include <sdr/bounded_queue.hpp>
#include <sdr/dsp.hpp>
#include <sdr/plugin.hpp>
#include <sdr/recording.hpp>
#include <thread>

class AudioBuffer {
  public:
    void push(std::span<const float>);
    qint64 read(char *, qint64);
    void clear(bool reset_metrics = true);
    std::uint64_t underruns() const { return underruns_; }
    std::uint64_t drops() const { return drops_; }

  private:
    std::mutex mutex_;
    std::vector<float> data_ = std::vector<float>(12000);
    std::size_t read_ = 0, size_ = 0;
    bool started_ = false;
    std::atomic<std::uint64_t> underruns_{0}, drops_{0};
};
struct Snapshot {
    QString state = "Idle", error;
    bool running = false, recording = false;
    std::vector<sdr::DeviceInfo> devices;
    QStringList demods{"AM", "NFM", "WFM"}, plugins;
    std::vector<float> spectrum;
    std::uint64_t frame = 0, iq_samples = 0, gaps = 0, overflows = 0;
    double sample_rate = 2400000, center_hz = 100000000, power_db = -120, processing_ms = 0;
    std::size_t queue_depth = 0;
};
class Engine {
  public:
    explicit Engine(AudioBuffer &);
    ~Engine();
    bool post(std::function<void()>);
    Snapshot snapshot();
    void scan();
    void start(const std::string &, sdr::Config);
    void stop();
    void record(const QString &, bool, bool);
    void stop_record(bool complete = true);
    void load_plugin(const QString &);
    void unload_plugins();
    sdr::Config config;
    std::string selected = "synthetic";
    std::string selected_demod;

  private:
    AudioBuffer &audio_;
    sdr::BoundedQueue<std::function<void()>> commands_{32};
    sdr::BoundedQueue<std::shared_ptr<const sdr::Block>> iq_{8};
    std::jthread worker_, acquisition_;
    std::mutex snapshot_mutex_, failure_mutex_;
    Snapshot snapshot_, current_;
    QString acquisition_failure_;
    std::atomic<bool> acquisition_done_{false};
    std::atomic<std::uint64_t> gaps_{0}, overflows_{0};
    std::vector<std::shared_ptr<sdr::PluginModule>> modules_;
    std::unique_ptr<sdr::PluginInstance> plugin_demod_;
    sdr::Demodulator demod_;
    std::vector<float> audio_block_;
    sdr::WavWriter wav_;
    sdr::IqWriter raw_;
    bool wav_on_ = false, raw_on_ = false;
    QString metadata_path_, session_id_;
    QJsonArray recording_gaps_;
    QJsonObject metadata_;
    std::chrono::steady_clock::time_point frame_time_{};
    void run(std::stop_token);
    void publish();
    void event(const QString &, const QString &);
    void metadata(bool);
};
