#ifndef SDR_DEVICE_PLUGIN_H
#define SDR_DEVICE_PLUGIN_H
#include "plugin.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SDR_DEVICE_ABI_MAJOR 1u
#define SDR_DEVICE_ABI_MINOR 0u
#define SDR_DEVICE_MAX_RATES 64u
#define SDR_DEVICE_GAIN 1u
#define SDR_DEVICE_PPM 2u
#define SDR_DEVICE_BIAS_T 4u
#define SDR_DEVICE_DIRECT_SAMPLING 8u
#define SDR_DEVICE_DISCONTINUITY 1u
typedef struct sdr_device_info {
    uint32_t struct_size;
    uint32_t capabilities;
    char key[1024];
    char label[256];
    uint32_t rate_count;
    double rates[SDR_DEVICE_MAX_RATES];
    double min_hz, max_hz, min_gain, max_gain;
} sdr_device_info;
typedef struct sdr_device_config {
    uint32_t struct_size;
    uint32_t bias_t;
    int32_t direct_sampling;
    double sample_rate, center_hz, gain, ppm;
} sdr_device_config;
typedef struct sdr_device_api {
    uint32_t struct_size, abi_major, abi_minor;
    const char *id;
    int32_t(SDR_CALL *enumerate)(uint32_t index, sdr_device_info *out);
    void *(SDR_CALL *create)(const char *key, const sdr_device_config *requested,
                             sdr_device_config *actual);
    void(SDR_CALL *destroy)(void *instance);
    int32_t(SDR_CALL *read)(void *instance, sdr_iq *out, uint32_t capacity, uint32_t *count,
                            uint32_t *flags);
    const char *(SDR_CALL *last_error)(void);
} sdr_device_api;
/* enumerate(0) starts a fresh snapshot, SDR_EOF terminates it (max 256 devices).
 * All strings are UTF-8 and NUL-terminated. Host owns descriptors/config/IQ buffers.
 * create/destroy own the opaque instance; borrowed pointers never outlive a call.
 * enumerate runs while RX is stopped. create/read/destroy run on one RX thread.
 * read returns SDR_OK with count=0 on timeout, or SDR_ERROR with last_error().
 * last_error is thread-local and valid until the next call; no exceptions cross ABI.
 * Native background threads/callbacks retained into host are not supported.
 */
typedef int32_t(SDR_CALL *sdr_device_plugin_init_fn)(uint32_t, uint32_t, sdr_device_api *);
SDR_EXPORT int32_t SDR_CALL sdr_device_plugin_init(uint32_t major, uint32_t minor,
                                                   sdr_device_api *out);
#ifdef __cplusplus
}
#endif
#endif
