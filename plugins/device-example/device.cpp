#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <sdr/device_plugin.h>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
thread_local char error[512]{};
void set_error(const char *message) {
    std::snprintf(error, sizeof(error), "%s", message);
}
struct Instance {
    sdr_device_config config;
    uint64_t index = 0;
};
int32_t SDR_CALL enumerate(uint32_t index, sdr_device_info *out) {
    if (!out || out->struct_size < sizeof(*out))
        return SDR_ERROR;
    if (index >= 2)
        return SDR_EOF;
    *out = {};
    out->struct_size = sizeof(*out);
    std::snprintf(out->key, sizeof(out->key), "example:%u", index);
    std::snprintf(out->label, sizeof(out->label), "Example device %u (generated IQ)", index);
    out->rate_count = 2;
    out->rates[0] = 240000;
    out->rates[1] = 2400000;
    out->min_hz = 0;
    out->max_hz = 6000000000;
    if (index == 0) {
        out->capabilities = 15;
        out->min_gain = 0;
        out->max_gain = 40;
    }
    return SDR_OK;
}
void *SDR_CALL create(const char *key, const sdr_device_config *requested,
                      sdr_device_config *actual) {
    try {
        error[0] = 0;
        if (!key || !requested || !actual || requested->struct_size < sizeof(*requested) ||
            actual->struct_size < sizeof(*actual) || !std::isfinite(requested->sample_rate) ||
            requested->sample_rate < 48000 || requested->sample_rate > 20000000 ||
            !std::isfinite(requested->center_hz) || requested->center_hz < 0 ||
            requested->center_hz > 6000000000 || !std::isfinite(requested->gain) ||
            !std::isfinite(requested->ppm) || requested->gain < 0 || requested->gain > 40 ||
            requested->bias_t > 1 || requested->direct_sampling < 0 ||
            requested->direct_sampling > 2)
            throw std::runtime_error("Example configuration invalid");
        const bool limited = std::string(key) == "example:1";
        if (!limited && std::string(key) != "example:0")
            throw std::runtime_error("Example device unavailable");
        if (limited && (requested->ppm != 0 || requested->bias_t || requested->direct_sampling))
            throw std::runtime_error("Setting unsupported by this example device");
        *actual = *requested;
        actual->sample_rate = std::round(actual->sample_rate / 1000) * 1000;
        actual->center_hz = std::round(actual->center_hz / 1000) * 1000;
        if (limited)
            actual->gain = 0;
        return new Instance{*actual};
    } catch (const std::exception &e) {
        set_error(e.what());
    } catch (...) {
        set_error("Example create failed");
    }
    return nullptr;
}
void SDR_CALL destroy(void *instance) {
    delete static_cast<Instance *>(instance);
}
int32_t SDR_CALL read(void *instance, sdr_iq *out, uint32_t capacity, uint32_t *count,
                      uint32_t *flags) {
    try {
        if (!instance || !out || !count || !flags || !capacity)
            return SDR_ERROR;
        auto &s = *static_cast<Instance *>(instance);
        *count = std::min({capacity, 16384u, static_cast<uint32_t>(s.config.sample_rate * 0.05)});
        *flags = 0;
        for (uint32_t i = 0; i < *count; ++i) {
            const double phase = 2 * std::numbers::pi * 1000 * s.index++ / s.config.sample_rate;
            out[i] = {static_cast<float>(0.5 * std::cos(phase)),
                      static_cast<float>(0.5 * std::sin(phase))};
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(*count / s.config.sample_rate));
        return SDR_OK;
    } catch (const std::exception &e) {
        set_error(e.what());
    } catch (...) {
        set_error("Example read failed");
    }
    return SDR_ERROR;
}
const char *SDR_CALL last_error() {
    return error;
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_device_plugin_init(uint32_t major, uint32_t minor,
                                                              sdr_device_api *out) {
    if (major != SDR_DEVICE_ABI_MAJOR || minor < SDR_DEVICE_ABI_MINOR || !out ||
        out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {sizeof(*out),
            SDR_DEVICE_ABI_MAJOR,
            SDR_DEVICE_ABI_MINOR,
            "org.sdr.example.device",
            enumerate,
            create,
            destroy,
            read,
            last_error};
    return SDR_OK;
}
