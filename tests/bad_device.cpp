#include <cstring>
#include <limits>
#include <sdr/device_plugin.h>
namespace {
int instance;
int32_t SDR_CALL enumerate(uint32_t index, sdr_device_info *out) {
    if (SDR_BAD_DEVICE == 10)
        return SDR_ERROR;
    if (index)
        return SDR_EOF;
    *out = {};
    out->struct_size = sizeof(*out);
    std::memcpy(out->key, "test:bad", sizeof("test:bad"));
    std::memcpy(out->label, "Fault injection", sizeof("Fault injection"));
    out->max_hz = 6000000000;
    if (SDR_BAD_DEVICE == 7)
        out->rate_count = SDR_DEVICE_MAX_RATES + 1;
    return SDR_OK;
}
void *SDR_CALL create(const char *, const sdr_device_config *requested, sdr_device_config *actual) {
    *actual = *requested;
    if (SDR_BAD_DEVICE == 8)
        actual->sample_rate = std::numeric_limits<double>::quiet_NaN();
    return &instance;
}
void SDR_CALL destroy(void *) {}
int32_t SDR_CALL read(void *, sdr_iq *out, uint32_t capacity, uint32_t *count, uint32_t *flags) {
    *count = 1;
    *flags = 0;
    out[0] = {0.25f, 0};
    if (SDR_BAD_DEVICE == 5)
        *count = capacity + 1;
    if (SDR_BAD_DEVICE == 6)
        out[0].i = std::numeric_limits<float>::quiet_NaN();
    return SDR_BAD_DEVICE == 9 ? SDR_ERROR : SDR_OK;
}
const char *SDR_CALL last_error() {
    return "Injected device failure";
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_device_plugin_init(uint32_t, uint32_t,
                                                              sdr_device_api *out) {
    if (!out || out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {sizeof(*out),
            SDR_DEVICE_ABI_MAJOR,
            SDR_DEVICE_ABI_MINOR,
            "org.sdr.test.device",
            enumerate,
            create,
            destroy,
            read,
            last_error};
    if (SDR_BAD_DEVICE == 0)
        out->abi_major = 99;
    if (SDR_BAD_DEVICE == 1)
        out->abi_minor = 99;
    if (SDR_BAD_DEVICE == 2)
        out->struct_size = sizeof(*out) - 1;
    if (SDR_BAD_DEVICE == 3)
        out->read = nullptr;
    return SDR_BAD_DEVICE == 4 ? SDR_ERROR : SDR_OK;
}
