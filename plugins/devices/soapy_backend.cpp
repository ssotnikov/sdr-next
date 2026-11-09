#include "backend.hpp"
#include "soapy_api.hpp"
#include <SoapySDR/Formats.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace device_backend {
namespace {
template <std::size_t N> void copy_text(char (&out)[N], const std::string &text) {
    if (text.size() >= N)
        throw std::runtime_error("Device key or label exceeds ABI capacity");
    std::memcpy(out, text.c_str(), text.size() + 1);
}
class Hardware final : public Receiver {
    SoapySDRDevice *device_ = nullptr;
    SoapySDRStream *stream_ = nullptr;
    bool gap_ = false;

  public:
    Hardware(const std::string &id, sdr_device_config &c) {
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
            if (gain_count) {
                checked(soapy::api().SoapySDRDevice_setGain(device_, SOAPY_SDR_RX, 0, c.gain));
                c.gain = soapy::api().SoapySDRDevice_getGain(device_, SOAPY_SDR_RX, 0);
            }
            if (soapy::api().SoapySDRDevice_hasFrequencyCorrection(device_, SOAPY_SDR_RX, 0)) {
                checked(soapy::api().SoapySDRDevice_setFrequencyCorrection(device_, SOAPY_SDR_RX, 0,
                                                                           c.ppm));
                c.ppm =
                    soapy::api().SoapySDRDevice_getFrequencyCorrection(device_, SOAPY_SDR_RX, 0);
            } else if (c.ppm != 0)
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
    void read(sdr_iq *out, uint32_t capacity, uint32_t &count, uint32_t &result_flags,
              sdr_device_read_metrics_v1 *metrics) override {
        void *buffers[] = {out};
        int flags = 0;
        long long time = 0;
        const auto read_stream = soapy::api().SoapySDRDevice_readStream;
        using Clock = std::chrono::steady_clock;
        const auto began = metrics ? Clock::now() : Clock::time_point{};
        const int n = read_stream(device_, stream_, buffers, capacity, &flags, &time, 100000);
        if (metrics) {
            metrics->driver_us =
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - began).count();
            metrics->driver_result = n;
        }
        count = 0;
        result_flags = 0;
        if (n == SOAPY_SDR_TIMEOUT)
            return;
        if (n == SOAPY_SDR_OVERFLOW) {
            gap_ = true;
            return;
        }
        if (n < 0)
            throw std::runtime_error(std::string("SDR read: ") + soapy::api().SoapySDR_errToStr(n));
        count = static_cast<uint32_t>(n);
        if (gap_)
            result_flags = SDR_DEVICE_DISCONTINUITY;
        gap_ = false;
    }
};

} // namespace
std::unique_ptr<Receiver> open(const std::string &key, sdr_device_config &actual) {
    return std::make_unique<Hardware>(key, actual);
}
std::vector<sdr_device_info> enumerate(const std::string &family) {
    auto &a = soapy::api();
    size_t count = 0;
    auto *list = a.SoapySDRDevice_enumerateStrArgs(("driver=" + family).c_str(), &count);
    std::vector<sdr_device_info> result;
    try {
        if (count > 256)
            throw std::runtime_error("Device count exceeds ABI capacity");
        for (size_t i = 0; i < count; ++i) {
            sdr_device_info info{};
            info.struct_size = sizeof(info);
            char *text = a.SoapySDRKwargs_toString(&list[i]);
            std::string key = text ? text : "";
            a.SoapySDR_free(text);
            copy_text(info.key, key);
            std::string label = family;
            for (size_t j = 0; j < list[i].size; ++j)
                if (std::string(list[i].keys[j]) == "label")
                    label = list[i].vals[j];
            copy_text(info.label, label);
            auto *device = a.SoapySDRDevice_make(&list[i]);
            if (!device)
                continue;
            try {
                size_t length = 0;
                double *rates = a.SoapySDRDevice_listSampleRates(device, SOAPY_SDR_RX, 0, &length);
                info.rate_count =
                    static_cast<uint32_t>(std::min<size_t>(length, SDR_DEVICE_MAX_RATES));
                if (rates)
                    std::copy_n(rates, info.rate_count, info.rates);
                else
                    info.rate_count = 0;
                a.SoapySDR_free(rates);
                auto *ranges = a.SoapySDRDevice_getFrequencyRange(device, SOAPY_SDR_RX, 0, &length);
                if (ranges && length) {
                    info.min_hz = ranges[0].minimum;
                    info.max_hz = ranges[length - 1].maximum;
                }
                a.SoapySDR_free(ranges);
                char **gains = a.SoapySDRDevice_listGains(device, SOAPY_SDR_RX, 0, &length);
                if (length) {
                    info.capabilities |= SDR_DEVICE_GAIN;
                    auto range = a.SoapySDRDevice_getGainRange(device, SOAPY_SDR_RX, 0);
                    info.min_gain = range.minimum;
                    info.max_gain = range.maximum;
                }
                a.SoapySDRStrings_clear(&gains, length);
                if (a.SoapySDRDevice_hasFrequencyCorrection(device, SOAPY_SDR_RX, 0))
                    info.capabilities |= SDR_DEVICE_PPM;
                auto *settings = a.SoapySDRDevice_getSettingInfo(device, &length);
                for (size_t j = 0; j < length; ++j) {
                    std::string option = settings[j].key;
                    if (option == "biastee" || option == "bias_tx")
                        info.capabilities |= SDR_DEVICE_BIAS_T;
                    if (option == "direct_samp")
                        info.capabilities |= SDR_DEVICE_DIRECT_SAMPLING;
                }
                a.SoapySDRArgInfoList_clear(settings, length);
                a.SoapySDRDevice_unmake(device);
            } catch (...) {
                a.SoapySDRDevice_unmake(device);
                throw;
            }
            result.push_back(info);
        }
    } catch (...) {
        a.SoapySDRKwargsList_clear(list, count);
        throw;
    }
    a.SoapySDRKwargsList_clear(list, count);
    return result;
}
} // namespace device_backend
