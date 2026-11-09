#pragma once
#include "engine.hpp"
#include <QAudioSink>
#include <QIODevice>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class AudioDevice final : public QIODevice {
    AudioBuffer &buffer_;

  public:
    explicit AudioDevice(AudioBuffer &buffer) : buffer_(buffer) {
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 48000 + QIODevice::bytesAvailable(); }
    qint64 readData(char *data, qint64 count) override { return buffer_.read(data, count); }
    qint64 writeData(const char *, qint64) override { return -1; }
};
class Controller final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList demods READ demods NOTIFY devicesChanged)
    Q_PROPERTY(QStringList audioOutputs READ audioOutputs CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settings CONSTANT)
  public:
    explicit Controller(QObject *parent = nullptr, bool trace_audio = false);
    ~Controller() override;
    QVariantMap status() const { return status_; }
    QVariantList devices() const { return devices_; }
    QStringList demods() const { return demods_; }
    QStringList audioOutputs() const;
    QVariantMap settings() const { return settings_; }
    Q_INVOKABLE void scan();
    Q_INVOKABLE void start(int, const QVariantMap &);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void record(const QString &, bool, bool);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void loadPlugin(const QString &);
    Q_INVOKABLE void unloadPlugins();
    Q_INVOKABLE void setAudio(int, double);
    Q_INVOKABLE void saveSettings(const QVariantMap &);
    Q_INVOKABLE void exportDiagnostics(const QString &);
    void reportTestFailure(const QString &message) { ui_error_ = message; }
    AudioTrace *audioTrace() const { return audio_.trace(); }
  signals:
    void changed();
    void devicesChanged();
    void spectrumReady(const QList<float> &);

  private:
    AudioBuffer audio_;
    Engine engine_;
    AudioDevice device_;
    std::unique_ptr<QAudioSink> sink_;
    QTimer poll_;
    QVariantMap status_, settings_;
    QVariantList devices_;
    QStringList demods_;
    Snapshot last_;
    QString settings_path_, ui_error_, audio_error_;
    int audio_index_ = 0;
    double volume_ = 0.5;
    bool audio_running_ = false;
    bool settings_invalid_ = false;
    std::uint64_t backend_underruns_ = 0;
    void poll();
    void startAudio();
    void dispatch(std::function<void()>);
};
