#include <cmath>
#include <new>
#include <sdr/plugin.h>
namespace {
struct State {
    double rate = 2400000, phase = 0, sum = 0, dc = 0;
    uint32_t n = 0;
};
void *SDR_CALL create() {
    return new (std::nothrow) State;
}
void SDR_CALL destroy(void *p) {
    delete static_cast<State *>(p);
}
int32_t SDR_CALL configure(void *p, const sdr_config *c) {
    if (!p || !c || c->struct_size < sizeof(*c) || c->audio_rate != 48000 ||
        !std::isfinite(c->sample_rate) || c->sample_rate < 48000 || c->offset_hz != 0)
        return SDR_ERROR;
    *static_cast<State *>(p) = {};
    static_cast<State *>(p)->rate = c->sample_rate;
    return SDR_OK;
}
int32_t SDR_CALL process(void *p, const sdr_iq *in, uint32_t n, float *out, uint32_t cap,
                         uint32_t *count) {
    if (!p || !in || !out || !count)
        return SDR_ERROR;
    auto &s = *static_cast<State *>(p);
    *count = 0;
    for (uint32_t i = 0; i < n; ++i) {
        s.sum += std::hypot(in[i].i, in[i].q);
        ++s.n;
        s.phase += 48000 / s.rate;
        if (s.phase >= 1) {
            s.phase -= 1;
            if (*count >= cap)
                return SDR_ERROR;
            const double envelope = s.sum / s.n;
            s.dc += 0.005 * (envelope - s.dc);
            out[(*count)++] = static_cast<float>(envelope - s.dc);
            s.sum = 0;
            s.n = 0;
        }
    }
    return SDR_OK;
}
} // namespace
extern "C" SDR_EXPORT int32_t SDR_CALL sdr_plugin_init(uint32_t major, uint32_t,
                                                       sdr_plugin_api *out) {
    if (major != SDR_ABI_MAJOR || !out || out->struct_size < sizeof(*out))
        return SDR_ERROR;
    *out = {sizeof(*out), SDR_ABI_MAJOR, SDR_ABI_MINOR, SDR_DEMODULATOR, "org.sdr.example.demod",
            create,       destroy,       configure,     nullptr,         process};
    return SDR_OK;
}
