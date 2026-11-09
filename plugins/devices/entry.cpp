#include "backend.hpp"
#include <cmath>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
thread_local char error[512]{};
std::vector<sdr_device_info> snapshot;
std::mutex snapshot_mutex;
void set_error(const char *message) {
    std::snprintf(error, sizeof(error), "%s", message);
}
int32_t SDR_CALL enumerate(uint32_t index, sdr_device_info *out) {
    try {
        error[0] = 0;
        std::lock_guard lock(snapshot_mutex);
        if (!out || out->struct_size < sizeof(*out))
            throw std::runtime_error("Invalid descriptor size");
        if (index == 0)
            snapshot = device_backend::enumerate(SDR_DEVICE_FAMILY);
        if (index >= snapshot.size())
            return SDR_EOF;
        *out = snapshot[index];
        return SDR_OK;
    } catch (const std::exception &e) {
        set_error(e.what());
    } catch (...) {
        set_error("Device enumeration failed");
    }
    return SDR_ERROR;
}
void *SDR_CALL create(const char *key, const sdr_device_config *requested,
                      sdr_device_config *actual) {
    try {
        error[0] = 0;
        if (!key || !requested || !actual || requested->struct_size < sizeof(*requested) ||
            actual->struct_size < sizeof(*actual) || !std::isfinite(requested->sample_rate) ||
            !std::isfinite(requested->center_hz) || !std::isfinite(requested->gain) ||
            !std::isfinite(requested->ppm) || requested->bias_t > 1 ||
            requested->direct_sampling < 0 || requested->direct_sampling > 2)
            throw std::runtime_error("Invalid device configuration");
        // A provider only opens devices returned by its own enumeration.
        const auto devices = device_backend::enumerate(SDR_DEVICE_FAMILY);
        bool found = false;
        for (const auto &device : devices)
            if (std::string(device.key) == key)
                found = true;
        if (!found)
            throw std::runtime_error("Device key is unavailable in this provider");
        *actual = *requested;
        return device_backend::open(key, *actual).release();
    } catch (const std::exception &e) {
        set_error(e.what());
    } catch (...) {
        set_error("Device open failed");
    }
    return nullptr;
}
void SDR_CALL destroy(void *instance) {
    delete static_cast<device_backend::Receiver *>(instance);
}
int32_t read_impl(void *instance, sdr_iq *out, uint32_t capacity, uint32_t *count,
                  uint32_t *flags, sdr_device_read_metrics_v1 *metrics) {
    try {
        error[0] = 0;
        if (metrics)
            *metrics = {-1, 0};
        if (!instance || !out || !capacity || !count || !flags)
            throw std::runtime_error("Invalid IQ buffers");
        *count = 0;
        *flags = 0;
        static_cast<device_backend::Receiver *>(instance)->read(out, capacity, *count, *flags,
                                                               metrics);
        return SDR_OK;
    } catch (const std::exception &e) {
        set_error(e.what());
    } catch (...) {
        set_error("Device read failed");
    }
    return SDR_ERROR;
}
int32_t SDR_CALL read(void *instance, sdr_iq *out, uint32_t capacity, uint32_t *count,
                      uint32_t *flags) {
    return read_impl(instance, out, capacity, count, flags, nullptr);
}
const char *SDR_CALL last_error() {
    return error;
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_device_read_diagnostics_v1(
    void *instance, sdr_iq *out, uint32_t capacity, uint32_t *count, uint32_t *flags,
    sdr_device_read_metrics_v1 *metrics) {
    return read_impl(instance, out, capacity, count, flags, metrics);
}

extern "C" SDR_EXPORT int32_t SDR_CALL sdr_device_plugin_init(uint32_t major, uint32_t minor,
                                                              sdr_device_api *out) {
    if (major != SDR_DEVICE_ABI_MAJOR || minor < SDR_DEVICE_ABI_MINOR || !out ||
        out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {sizeof(*out),
            SDR_DEVICE_ABI_MAJOR,
            SDR_DEVICE_ABI_MINOR,
            SDR_DEVICE_ID,
            enumerate,
            create,
            destroy,
            read,
            last_error};
    return SDR_OK;
}
