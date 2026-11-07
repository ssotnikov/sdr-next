#ifndef SDR_PLUGIN_H
#define SDR_PLUGIN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SDR_ABI_MAJOR 1u
#define SDR_ABI_MINOR 0u
#define SDR_SOURCE 1u
#define SDR_DEMODULATOR 2u
#define SDR_OK 0
#define SDR_ERROR -1
#define SDR_EOF 1
#if defined(_WIN32)
#define SDR_EXPORT __declspec(dllexport)
#define SDR_CALL __cdecl
#else
#define SDR_EXPORT __attribute__((visibility("default")))
#define SDR_CALL
#endif
typedef struct sdr_iq {
    float i;
    float q;
} sdr_iq;
typedef struct sdr_config {
    uint32_t struct_size;
    uint32_t audio_rate;
    double sample_rate;
    double center_hz;
    double offset_hz;
} sdr_config;
typedef struct sdr_plugin_api {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t kind;
    const char *id;
    void *(SDR_CALL *create)(void);
    void(SDR_CALL *destroy)(void *);
    int32_t(SDR_CALL *configure)(void *, const sdr_config *);
    int32_t(SDR_CALL *read)(void *, sdr_iq *, uint32_t, uint32_t *, uint32_t *);
    int32_t(SDR_CALL *process)(void *, const sdr_iq *, uint32_t, float *, uint32_t, uint32_t *);
} sdr_plugin_api;
/* Caller zeroes out_api and sets struct_size. Returns SDR_ERROR for unsupported major.
 * All callbacks are synchronous on one worker thread; no retained borrowed pointers,
 * background threads or callbacks into host. Every function contains exceptions/panics.
 * The library stays loaded until every created instance has been destroyed. */
typedef int32_t(SDR_CALL *sdr_plugin_init_fn)(uint32_t, uint32_t, sdr_plugin_api *);
SDR_EXPORT int32_t SDR_CALL sdr_plugin_init(uint32_t major, uint32_t minor,
                                            sdr_plugin_api *out_api);
#ifdef __cplusplus
}
#endif
#endif
