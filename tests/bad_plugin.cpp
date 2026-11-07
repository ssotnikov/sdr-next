#include <sdr/plugin.h>
#ifndef SDR_BAD_CASE
#define SDR_BAD_CASE 0
#endif
namespace {
int object;
void *SDR_CALL create() {
    return &object;
}
void SDR_CALL destroy(void *) {}
int32_t SDR_CALL configure(void *, const sdr_config *) {
    return SDR_OK;
}
int32_t SDR_CALL read(void *, sdr_iq *, uint32_t, uint32_t *count, uint32_t *) {
    *count = 0;
    return SDR_OK;
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_plugin_init(uint32_t, uint32_t, sdr_plugin_api *out) {
    if (!out || out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {};
    out->struct_size = sizeof(*out);
    out->abi_major = SDR_BAD_CASE == 0 ? 99 : SDR_ABI_MAJOR;
    out->abi_minor = SDR_BAD_CASE == 1 ? 99 : SDR_ABI_MINOR;
    out->kind = SDR_SOURCE;
    out->id = "org.sdr.example.bad";
    out->create = create;
    out->destroy = destroy;
    out->configure = configure;
    out->read = SDR_BAD_CASE == 3 ? nullptr : read;
    if (SDR_BAD_CASE == 2)
        out->struct_size = sizeof(*out) - 1;
    return SDR_BAD_CASE == 4 ? SDR_ERROR : SDR_OK;
}
