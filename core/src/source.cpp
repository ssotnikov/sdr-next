#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sdr/path.hpp>
#include <sdr/source.hpp>
#include <stdexcept>
#ifdef SDR_HAS_SOAPY
#include "soapy_api.hpp"
#include <SoapySDR/Device.h>
#include <SoapySDR/Errors.h>
#include <SoapySDR/Formats.h>
#endif

namespace sdr {
namespace {
class Synthetic final : public Source {
    Config config_;
    double phase_ = 0;
    std::uint64_t index_ = 0, sequence_ = 0;

  public:
    explicit Synthetic(Config c) : config_(std::move(c)) {}
    bool read(Block &b) override {
        b.samples.resize(block_size);
        b.sample_index = index_;
        b.sequence = sequence_++;
        b.discontinuity = false;
        for (auto &s : b.samples) {
            const double t = static_cast<double>(index_++) / config_.sample_rate;
            const double tone = std::sin(2 * std::numbers::pi * 1000 * t);
            if (config_.mode == "AM") {
                s = std::polar(static_cast<float>(0.5 * (1 + 0.5 * tone)),
                               static_cast<float>(2 * std::numbers::pi * config_.offset_hz * t));
            } else {
                phase_ += 2 * std::numbers::pi *
                          (config_.offset_hz + (config_.mode == "WFM" ? 50000 : 2500) * tone) /
                          config_.sample_rate;
                phase_ = std::remainder(phase_, 2 * std::numbers::pi);
                s = std::polar(0.5f, static_cast<float>(phase_));
            }
        }
        return true;
    }
};
float read_float(const char *bytes) {
    std::uint32_t u = 0;
    for (unsigned j = 0; j < 4; ++j)
        u |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[j])) << (8 * j);
    return std::bit_cast<float>(u);
}
class File final : public Source {
    std::ifstream stream_;
    std::vector<char> buffer_ = std::vector<char>(block_size * 8);
    std::uint64_t index_ = 0, sequence_ = 0;

  public:
    explicit File(const std::string &path) : stream_(path_from_utf8(path), std::ios::binary) {
        if (!stream_)
            throw std::runtime_error("Cannot open IQ file");
    }
    bool read(Block &b) override {
        stream_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        const auto bytes = static_cast<std::size_t>(stream_.gcount());
        if (stream_.bad())
            throw std::runtime_error("IQ file read error");
        if (!bytes)
            return false;
        if (bytes % 8)
            throw std::runtime_error("Truncated CF32 IQ sample");
        b.sample_index = index_;
        b.sequence = sequence_++;
        b.discontinuity = false;
        b.samples.resize(bytes / 8);
        for (std::size_t i = 0; i < b.samples.size(); ++i) {
            const float re = read_float(buffer_.data() + i * 8),
                        im = read_float(buffer_.data() + i * 8 + 4);
            if (!std::isfinite(re) || !std::isfinite(im))
                throw std::runtime_error("Non-finite IQ sample");
            b.samples[i] = {re, im};
        }
        index_ += b.samples.size();
        return true;
    }
};
#ifdef SDR_HAS_SOAPY
class Hardware final : public Source {
    SoapySDRDevice *device_ = nullptr;
    SoapySDRStream *stream_ = nullptr;
    std::uint64_t index_ = 0, sequence_ = 0;
    bool gap_ = false;

  public:
    Hardware(const std::string &id, Config &c) {
        try {
            device_ = soapy::api().SoapySDRDevice_makeStrArgs(id.c_str());
            if (!device_)
                throw std::runtime_error("SDR device could not be opened");
            auto checked = [](int result) {
                if (result != 0)
                    throw std::runtime_error(soapy::api().SoapySDRDevice_lastError());
            };
            checked(
                soapy::api().SoapySDRDevice_setSampleRate(device_, SOAPY_SDR_RX, 0, c.sample_rate));
            c.sample_rate = soapy::api().SoapySDRDevice_getSampleRate(device_, SOAPY_SDR_RX, 0);
            checked(soapy::api().SoapySDRDevice_setFrequency(device_, SOAPY_SDR_RX, 0, c.center_hz,
                                                             nullptr));
            c.center_hz = soapy::api().SoapySDRDevice_getFrequency(device_, SOAPY_SDR_RX, 0);
            size_t gain_count = 0;
            char **gains =
                soapy::api().SoapySDRDevice_listGains(device_, SOAPY_SDR_RX, 0, &gain_count);
            soapy::api().SoapySDRStrings_clear(&gains, gain_count);
            if (gain_count)
                checked(soapy::api().SoapySDRDevice_setGain(device_, SOAPY_SDR_RX, 0, c.gain));
            if (soapy::api().SoapySDRDevice_hasFrequencyCorrection(device_, SOAPY_SDR_RX, 0))
                checked(soapy::api().SoapySDRDevice_setFrequencyCorrection(device_, SOAPY_SDR_RX, 0,
                                                                           c.ppm));
            else if (c.ppm != 0)
                throw std::runtime_error("Frequency correction is unsupported");
            bool has_bias = false, has_direct = false;
            std::string bias_key;
            size_t setting_count = 0;
            auto *settings = soapy::api().SoapySDRDevice_getSettingInfo(device_, &setting_count);
            for (size_t i = 0; i < setting_count; ++i) {
                const std::string key = settings[i].key;
                if (key == "biastee" || key == "bias_tx") {
                    has_bias = true;
                    bias_key = key;
                }
                has_direct |= std::string(settings[i].key) == "direct_samp";
            }
            soapy::api().SoapySDRArgInfoList_clear(settings, setting_count);
            if (has_bias)
                checked(soapy::api().SoapySDRDevice_writeSetting(device_, bias_key.c_str(),
                                                                 c.bias_t ? "true" : "false"));
            if (has_direct)
                checked(soapy::api().SoapySDRDevice_writeSetting(
                    device_, "direct_samp", std::to_string(c.direct_sampling).c_str()));
            if (c.bias_t && !has_bias)
                throw std::runtime_error("Bias-T is unsupported");
            if (c.direct_sampling && !has_direct)
                throw std::runtime_error("Direct sampling is unsupported");
            const size_t channel = 0;
            stream_ = soapy::api().SoapySDRDevice_setupStream(device_, SOAPY_SDR_RX, SOAPY_SDR_CF32,
                                                              &channel, 1, nullptr);
            if (!stream_)
                throw std::runtime_error(soapy::api().SoapySDRDevice_lastError());
            if (soapy::api().SoapySDRDevice_activateStream(device_, stream_, 0, 0, 0) != 0)
                throw std::runtime_error("SDR stream activation failed");
        } catch (...) {
            cleanup();
            throw;
        }
    }
    void cleanup() noexcept {
        if (device_) {
            if (stream_) {
                soapy::api().SoapySDRDevice_deactivateStream(device_, stream_, 0, 0);
                soapy::api().SoapySDRDevice_closeStream(device_, stream_);
            }
            soapy::api().SoapySDRDevice_unmake(device_);
        }
        device_ = nullptr;
        stream_ = nullptr;
    }
    ~Hardware() override { cleanup(); }
    bool paced() const override { return false; }
    bool read(Block &b) override {
        b.samples.resize(block_size);
        void *buffers[] = {b.samples.data()};
        int flags = 0;
        long long time = 0;
        const int count = soapy::api().SoapySDRDevice_readStream(device_, stream_, buffers,
                                                                 block_size, &flags, &time, 100000);
        b.sample_index = index_;
        b.sequence = sequence_++;
        b.discontinuity = gap_;
        gap_ = false;
        if (count == SOAPY_SDR_TIMEOUT) {
            b.samples.clear();
            return true;
        }
        if (count == SOAPY_SDR_OVERFLOW) {
            gap_ = true;
            b.discontinuity = true;
            b.samples.clear();
            return true;
        }
        if (count < 0)
            throw std::runtime_error(std::string("SDR read: ") +
                                     soapy::api().SoapySDR_errToStr(count));
        b.samples.resize(static_cast<std::size_t>(count));
        index_ += count;
        return true;
    }
};
#endif
} // namespace
bool hardware_available() {
#ifdef SDR_HAS_SOAPY
    try {
        (void)soapy::api();
        return true;
    } catch (const std::exception &) {
        return false;
    }
#else
    return false;
#endif
}
std::vector<DeviceInfo> enumerate_devices() {
    std::vector<DeviceInfo> devices;
    devices.push_back({"synthetic",
                       "Synthetic / 1 kHz",
                       {2400000, 10000000, 20000000},
                       0,
                       6000000000,
                       0,
                       0,
                       false,
                       false,
                       false,
                       false});
    devices.push_back({"file",
                       "Raw IQ / CF32 little-endian",
                       {},
                       0,
                       6000000000,
                       0,
                       0,
                       false,
                       false,
                       false,
                       false});
#ifdef SDR_HAS_SOAPY
    if (!hardware_available())
        return devices;
    for (const std::string family : {"hackrf", "rtlsdr", "airspy"}) {
        size_t count = 0;
        auto *list =
            soapy::api().SoapySDRDevice_enumerateStrArgs(("driver=" + family).c_str(), &count);
        for (size_t i = 0; i < count; ++i) {
            DeviceInfo info;
            char *text = soapy::api().SoapySDRKwargs_toString(&list[i]);
            info.id = "soapy:" + std::string(text ? text : "");
            soapy::api().SoapySDR_free(text);
            info.label = family;
            for (size_t j = 0; j < list[i].size; ++j)
                if (std::string(list[i].keys[j]) == "label")
                    info.label = list[i].vals[j];
            auto *d = soapy::api().SoapySDRDevice_make(&list[i]);
            if (!d)
                continue;
            size_t length = 0;
            double *rates =
                soapy::api().SoapySDRDevice_listSampleRates(d, SOAPY_SDR_RX, 0, &length);
            if (rates)
                info.rates.assign(rates, rates + length);
            soapy::api().SoapySDR_free(rates);
            auto *ranges =
                soapy::api().SoapySDRDevice_getFrequencyRange(d, SOAPY_SDR_RX, 0, &length);
            if (ranges && length) {
                info.min_hz = ranges[0].minimum;
                info.max_hz = ranges[length - 1].maximum;
            }
            soapy::api().SoapySDR_free(ranges);
            char **gains = soapy::api().SoapySDRDevice_listGains(d, SOAPY_SDR_RX, 0, &length);
            info.gain = length > 0;
            soapy::api().SoapySDRStrings_clear(&gains, length);
            if (info.gain) {
                const auto range = soapy::api().SoapySDRDevice_getGainRange(d, SOAPY_SDR_RX, 0);
                info.min_gain = range.minimum;
                info.max_gain = range.maximum;
            }
            info.ppm = soapy::api().SoapySDRDevice_hasFrequencyCorrection(d, SOAPY_SDR_RX, 0);
            auto *settings = soapy::api().SoapySDRDevice_getSettingInfo(d, &length);
            for (size_t j = 0; j < length; ++j) {
                const std::string key = settings[j].key;
                info.bias_t |= key == "biastee" || key == "bias_tx";
                info.direct_sampling |= key == "direct_samp";
            }
            soapy::api().SoapySDRArgInfoList_clear(settings, length);
            soapy::api().SoapySDRDevice_unmake(d);
            devices.push_back(std::move(info));
        }
        soapy::api().SoapySDRKwargsList_clear(list, count);
    }
#endif
    return devices;
}
std::unique_ptr<Source> open_source(const std::string &id, Config &c) {
    if (!std::isfinite(c.sample_rate) || c.sample_rate < 48000 || c.sample_rate > 20000000 ||
        !std::isfinite(c.center_hz) || c.center_hz < 0 || !std::isfinite(c.gain) ||
        !std::isfinite(c.ppm))
        throw std::invalid_argument("Invalid source configuration");
    if (id == "synthetic")
        return std::make_unique<Synthetic>(c);
    if (id == "file")
        return std::make_unique<File>(c.file);
#ifdef SDR_HAS_SOAPY
    if (id.starts_with("soapy:"))
        return std::make_unique<Hardware>(id.substr(6), c);
#endif
    throw std::invalid_argument("Unknown source or SoapySDR support unavailable");
}
} // namespace sdr
