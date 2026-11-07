#include <cmath>
#include <new>
#include <numbers>
#include <sdr/plugin.h>
namespace {
struct State {
    double rate = 2400000, phase = 0;
};
void *SDR_CALL create() {
    return new (std::nothrow) State;
}
void SDR_CALL destroy(void *p) {
    delete static_cast<State *>(p);
}
int32_t SDR_CALL configure(void *p, const sdr_config *c) {
    if (!p || !c || c->struct_size < sizeof(*c) || !std::isfinite(c->sample_rate) ||
        c->sample_rate < 48000)
        return SDR_ERROR;
    static_cast<State *>(p)->rate = c->sample_rate;
    return SDR_OK;
}
int32_t SDR_CALL read(void *p, sdr_iq *out, uint32_t capacity, uint32_t *count, uint32_t *flags) {
    if (!p || !out || !count || !flags)
        return SDR_ERROR;
    auto &s = *static_cast<State *>(p);
    for (uint32_t i = 0; i < capacity; ++i) {
        out[i] = {static_cast<float>(0.5 * std::cos(s.phase)),
                  static_cast<float>(0.5 * std::sin(s.phase))};
        s.phase =
            std::remainder(s.phase + 2 * std::numbers::pi * 1000 / s.rate, 2 * std::numbers::pi);
    }
    *count = capacity;
    *flags = 0;
    return SDR_OK;
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_plugin_init(uint32_t major, uint32_t,
                                                       sdr_plugin_api *out) {
    if (major != SDR_ABI_MAJOR || !out || out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {sizeof(*out), SDR_ABI_MAJOR, SDR_ABI_MINOR, SDR_SOURCE, "org.sdr.example.source",
            create,       destroy,       configure,     read,       nullptr};
    return SDR_OK;
}
