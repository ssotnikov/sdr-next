#include "controller.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMediaDevices>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {
QString localPath(const QString &value) {
    const QUrl url(value);
    return url.isLocalFile() ? url.toLocalFile() : value;
}
} // namespace
Controller::Controller(QObject *parent, bool trace_audio, bool adaptive_audio)
    : QObject(parent), audio_(trace_audio, adaptive_audio), engine_(audio_), device_(audio_) {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    settings_path_ = directory + "/settings.json";
    if (QFile::exists(QCoreApplication::applicationDirPath() + "/portable.txt"))
        settings_path_ = QCoreApplication::applicationDirPath() + "/sdr-settings.json";
    QFile file(settings_path_);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
            ui_error_ = "Cannot read settings; original file was preserved";
        else {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject() ||
                doc.object()["schema_version"].toInt() != 1)
                ui_error_ = "Invalid or unsupported settings; original file was preserved";
            else
                settings_ = doc.object()["settings"].toObject().toVariantMap();
        }
    }
    settings_invalid_ = !ui_error_.isEmpty();
    connect(&poll_, &QTimer::timeout, this, &Controller::poll);
    poll_.start(33);
    poll();
}
Controller::~Controller() {
    if (sink_)
        sink_->stop();
}
QStringList Controller::audioOutputs() const {
    QStringList result{"System default"};
    for (const auto &d : QMediaDevices::audioOutputs())
        result.push_back(d.description());
    return result;
}
void Controller::dispatch(std::function<void()> command) {
    if (!engine_.post(std::move(command))) {
        ui_error_ = "Command queue is full; retry after current operation";
        emit changed();
    }
}
void Controller::scan() {
    dispatch([this] { engine_.scan(); });
}
void Controller::start(int index, const QVariantMap &values) {
    if (index < 0 || index >= static_cast<int>(last_.devices.size())) {
        ui_error_ = "Select an available source";
        emit changed();
        return;
    }
    ui_error_.clear();
    audio_error_.clear();
    const auto id = last_.devices[static_cast<std::size_t>(index)].id;
    sdr::Config c;
    c.center_hz = values.value("centerMHz").toDouble() * 1e6;
    c.sample_rate = values.value("rate").toDouble();
    c.offset_hz = values.value("offsetKHz").toDouble() * 1000;
    c.gain = values.value("gain").toDouble();
    c.ppm = values.value("ppm").toDouble();
    c.bias_t = values.value("biasT").toBool();
    c.direct_sampling = values.value("directSampling").toInt();
    c.squelch_db = values.value("squelch", -100).toDouble();
    c.file = localPath(values.value("file").toString()).toUtf8().toStdString();
    const auto mode = values.value("mode", "WFM").toString().toStdString();
    c.mode = (mode == "AM" || mode == "NFM" || mode == "WFM") ? mode : "AM";
    if (!std::isfinite(c.center_hz) || !std::isfinite(c.sample_rate) ||
        !std::isfinite(c.offset_hz) || !std::isfinite(c.squelch_db)) {
        ui_error_ = "Frequency and sample rate must be finite numbers";
        emit changed();
        return;
    }
    dispatch([this, id, c, mode] {
        engine_.selected_demod = (mode == "AM" || mode == "NFM" || mode == "WFM") ? "" : mode;
        engine_.start(id, c);
    });
}
void Controller::stop() {
    dispatch([this] { engine_.stop(); });
}
void Controller::record(const QString &folder, bool wav, bool iq) {
    const auto dir = localPath(folder);
    dispatch([this, dir, wav, iq] { engine_.record(dir, wav, iq); });
}
void Controller::stopRecording() {
    dispatch([this] { engine_.stop_record(); });
}
void Controller::loadPlugin(const QString &manifest) {
    const auto file = localPath(manifest);
    dispatch([this, file] { engine_.load_plugin(file); });
}
void Controller::unloadPlugins() {
    dispatch([this] { engine_.unload_plugins(); });
}
void Controller::startAudio() {
    if (sink_) {
        sink_->stop();
        sink_.reset();
    }
    audio_.clear(false);
    const auto outputs = QMediaDevices::audioOutputs();
    const auto output = audio_index_ > 0 && audio_index_ <= outputs.size()
                            ? outputs[audio_index_ - 1]
                            : QMediaDevices::defaultAudioOutput();
    QAudioFormat format;
    format.setSampleRate(sdr::audio_rate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    if (output.isNull() || !output.isFormatSupported(format)) {
        audio_error_ = "Audio output does not support mono float32 / 48 kHz; choose another device";
        return;
    }
    sink_ = std::make_unique<QAudioSink>(output, format);
    sink_->setBufferSize(2400 * sizeof(float));
    sink_->setVolume(static_cast<float>(volume_));
    connect(sink_.get(), &QAudioSink::stateChanged, this, [this](QtAudio::State state) {
        if (auto *trace = audio_.trace(); trace && sink_)
            trace->add({AudioTrace::Kind::sink_state, 0, static_cast<std::size_t>(state),
                        static_cast<std::size_t>(sink_->error())});
        if (state == QtAudio::IdleState && sink_ && sink_->error() == QtAudio::UnderrunError)
            ++backend_underruns_;
        if (state == QtAudio::StoppedState && sink_ && sink_->error() != QtAudio::NoError) {
            audio_error_ = "Audio output was lost or could not be opened";
            emit changed();
        }
    });
    sink_->start(&device_);
}
void Controller::setAudio(int index, double volume) {
    const bool changed = index != audio_index_;
    audio_index_ = index;
    volume_ = std::clamp(volume, 0.0, 1.0);
    if (last_.running && changed)
        startAudio();
    else if (sink_)
        sink_->setVolume(static_cast<float>(volume_));
}
void Controller::saveSettings(const QVariantMap &values) {
    if (QCoreApplication::arguments().contains("--endurance-dir"))
        return;
    // Preserve malformed/unknown files until the user explicitly removes or repairs them.
    if (settings_invalid_)
        return;
    QDir().mkpath(QFileInfo(settings_path_).absolutePath());
    QSaveFile file(settings_path_);
    const auto data = QJsonDocument(QJsonObject{{"schema_version", 1},
                                                {"settings", QJsonObject::fromVariantMap(values)}})
                          .toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        ui_error_ = "Settings could not be saved";
        emit changed();
    }
}
void Controller::exportDiagnostics(const QString &destination) {
    const QString source = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                           "/diagnostics.jsonl";
    if (!QFile::copy(source, localPath(destination))) {
        ui_error_ = "Diagnostics could not be exported; destination must not already exist";
        emit changed();
    }
}
void Controller::poll() {
    const auto next = engine_.snapshot();
    QVariantList devices;
    for (const auto &d : next.devices) {
        QVariantList rates;
        for (double rate : d.rates)
            rates.push_back(rate);
        devices.push_back(QVariantMap{{"id", QString::fromStdString(d.id)},
                                      {"label", QString::fromStdString(d.label)},
                                      {"rates", rates},
                                      {"gain", d.gain},
                                      {"minGain", d.min_gain},
                                      {"maxGain", d.max_gain},
                                      {"minHz", d.min_hz},
                                      {"maxHz", d.max_hz},
                                      {"ppm", d.ppm},
                                      {"biasT", d.bias_t},
                                      {"directSampling", d.direct_sampling}});
    }
    if (devices != devices_ || next.demods != demods_) {
        devices_ = devices;
        demods_ = next.demods;
        emit devicesChanged();
    }
    if (next.running && !audio_running_) {
        backend_underruns_ = 0;
        startAudio();
    }
    if (!next.running && audio_running_ && sink_) {
        sink_->stop();
        sink_.reset();
        audio_.clear(false);
    }
    audio_running_ = next.running;
    QString error = !ui_error_.isEmpty()      ? ui_error_
                    : !audio_error_.isEmpty() ? audio_error_
                                              : next.error;
    const auto audio_metrics = audio_.metrics();
    status_ = {{"state", next.state},
               {"error", error},
               {"running", next.running},
               {"recording", next.recording},
               {"samples", static_cast<qulonglong>(next.iq_samples)},
               {"gaps", static_cast<qulonglong>(next.gaps)},
               {"overflows", static_cast<qulonglong>(next.overflows)},
               {"rate", next.sample_rate},
               {"centerHz", next.center_hz},
               {"gain", next.gain},
               {"ppm", next.ppm},
               {"biasT", next.bias_t},
               {"directSampling", next.direct_sampling},
               {"deviceModules", next.device_modules},
               {"deviceModuleErrors", next.device_module_errors},
               {"powerDb", next.power_db},
               {"processingMs", next.processing_ms},
               {"maxSourceReadMs", next.max_source_read_ms},
               {"maxIqEnqueueWaitMs", next.max_enqueue_wait_ms},
               {"maxDspMs", next.max_dsp_ms},
               {"maxRecordWriteMs", next.max_record_write_ms},
               {"maxWorkerBlockGapMs", next.max_worker_gap_ms},
               {"maxSnapshotPublishMs", next.max_publish_ms},
               {"maxSpectrumMs", next.max_spectrum_ms},
               {"maxAudioPushMs", next.max_audio_push_ms},
               {"maxBlockMs", next.max_block_ms},
               {"idleWaitEntries", static_cast<qulonglong>(next.idle_waits)},
               {"queueDepth", static_cast<qulonglong>(next.queue_depth)},
               {"audioUnderruns", static_cast<qulonglong>(audio_.underruns() + backend_underruns_)},
               {"audioBackendUnderruns", static_cast<qulonglong>(backend_underruns_)},
               {"audioDrops", static_cast<qulonglong>(audio_.drops())},
               {"audioFramesInput", static_cast<qulonglong>(audio_metrics.input)},
               {"audioFramesAccepted", static_cast<qulonglong>(audio_metrics.accepted)},
               {"audioFramesRequested", static_cast<qulonglong>(audio_metrics.requested)},
               {"audioFramesDelivered", static_cast<qulonglong>(audio_metrics.delivered)},
               {"audioFramesConsumed", static_cast<qulonglong>(audio_metrics.consumed)},
               {"audioFramesDiscarded", static_cast<qulonglong>(audio_metrics.discarded)},
               {"audioRateCorrectionPpm", audio_metrics.correction_ppm},
               {"audioQueuedFrames", static_cast<qulonglong>(audio_metrics.queued)},
               {"maxAudioReadGapMs", audio_metrics.max_read_gap_ms},
               {"maxAudioPushGapMs", audio_metrics.max_push_gap_ms},
               {"audioSinkState", sink_ ? static_cast<int>(sink_->state()) : -1},
               {"audioSinkProcessedUs", sink_ ? sink_->processedUSecs() : qint64(0)},
               {"audioSinkElapsedUs", sink_ ? sink_->elapsedUSecs() : qint64(0)},
               {"audioSinkBytesFree", sink_ ? sink_->bytesFree() : qsizetype(0)},
               {"audioDeviceBufferFrames",
                sink_ ? sink_->bufferSize() / static_cast<qsizetype>(sizeof(float)) : 0},
               {"frames", static_cast<qulonglong>(next.frame)},
               {"plugins", next.plugins}};
    if (next.frame != last_.frame && !next.spectrum.empty())
        emit spectrumReady(QList<float>(next.spectrum.begin(), next.spectrum.end()));
    last_ = next;
    emit changed();
}
