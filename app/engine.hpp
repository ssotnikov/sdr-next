#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <sdr/bounded_queue.hpp>
#include <sdr/device.hpp>
#include <sdr/dsp.hpp>
#include <sdr/plugin.hpp>
#include <sdr/recording.hpp>
#include <thread>

class AudioBuffer {
  public:
    struct Metrics {
        std::uint64_t input = 0, accepted = 0, requested = 0, delivered = 0, discarded = 0;
        std::size_t queued = 0;
        double max_read_gap_ms = 0, max_push_gap_ms = 0;
    };
    Metrics metrics();
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
    Metrics metrics_;
    std::chrono::steady_clock::time_point last_read_{}, last_push_{};
};
struct Snapshot {
    QString state = "Idle", error;
    bool running = false, recording = false;
    std::vector<sdr::DeviceInfo> devices;
    QStringList demods{"AM", "NFM", "WFM"}, plugins;
    QStringList device_modules, device_module_errors;
    std::vector<float> spectrum;
    std::uint64_t frame = 0, iq_samples = 0, gaps = 0, overflows = 0;
    double sample_rate = 2400000, center_hz = 100000000, power_db = -120, processing_ms = 0;
    double gain = 0, ppm = 0;
    bool bias_t = false;
    int direct_sampling = 0;
    std::size_t queue_depth = 0;
    double max_source_read_ms = 0, max_enqueue_wait_ms = 0, max_dsp_ms = 0;
    double max_record_write_ms = 0, max_worker_gap_ms = 0, max_publish_ms = 0;
    double max_spectrum_ms = 0, max_audio_push_ms = 0, max_block_ms = 0;
    std::uint64_t idle_waits = 0;
};
class Engine {
  public:
    explicit Engine(AudioBuffer &, const QString &device_directory = {});
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
    std::mutex wake_mutex_;
    std::condition_variable_any wake_;
    std::atomic<std::uint64_t> work_revision_{0};
    Snapshot snapshot_, current_;
    QString acquisition_failure_;
    std::atomic<bool> acquisition_done_{false};
    std::atomic<std::uint64_t> gaps_{0}, overflows_{0};
    std::atomic<double> max_source_read_ms_{0}, max_enqueue_wait_ms_{0};
    std::vector<std::shared_ptr<sdr::PluginModule>> modules_;
    struct DeviceBinding {
        std::shared_ptr<sdr::DeviceModule> module;
        std::string key;
    };
    std::map<std::string, DeviceBinding> device_registry_;
    std::vector<std::shared_ptr<sdr::DeviceModule>> device_modules_;
    QString device_directory_;
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
    std::chrono::steady_clock::time_point last_block_time_{};
    void run(std::stop_token);
    void publish();
    void event(const QString &, const QString &);
    void metadata(bool);
    void notify_work();
};
