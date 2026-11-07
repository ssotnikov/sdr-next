#pragma once
#include <SoapySDR/Device.h>
#include <SoapySDR/Errors.h>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Resolve the vendor's public C ABI so file/synthetic RX remains usable even
// when no separately installed hardware runtime is present.
namespace soapy {
// clang-format off
#define SDR_SOAPY_SYMBOLS(X) \
    X(SoapySDRDevice_makeStrArgs) \
    X(SoapySDRDevice_make) \
    X(SoapySDRDevice_unmake) \
    X(SoapySDRDevice_setSampleRate) \
    X(SoapySDRDevice_getSampleRate) \
    X(SoapySDRDevice_setFrequency) \
    X(SoapySDRDevice_getFrequency) \
    X(SoapySDRDevice_listGains) \
    X(SoapySDRDevice_setGain) \
    X(SoapySDRDevice_getGainRange) \
    X(SoapySDRDevice_hasFrequencyCorrection) \
    X(SoapySDRDevice_setFrequencyCorrection) \
    X(SoapySDRDevice_getSettingInfo) \
    X(SoapySDRDevice_writeSetting) \
    X(SoapySDRDevice_setupStream) \
    X(SoapySDRDevice_activateStream) \
    X(SoapySDRDevice_deactivateStream) \
    X(SoapySDRDevice_closeStream) \
    X(SoapySDRDevice_readStream) \
    X(SoapySDRDevice_enumerateStrArgs) \
    X(SoapySDRDevice_listSampleRates) \
    X(SoapySDRDevice_getFrequencyRange) \
    X(SoapySDRDevice_lastError) \
    X(SoapySDRKwargs_toString) \
    X(SoapySDRKwargsList_clear) \
    X(SoapySDRStrings_clear) \
    X(SoapySDRArgInfoList_clear) \
    X(SoapySDR_free) \
    X(SoapySDR_errToStr)
// clang-format on
class Api {
    void *library_ = nullptr;
    void *symbol(const char *name) {
#ifdef _WIN32
        auto *result =
            reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(library_), name));
#else
        auto *result = dlsym(library_, name);
#endif
        if (!result)
            throw std::runtime_error(std::string("Missing SoapySDR 0.8 symbol: ") + name);
        return result;
    }
    void close() {
        if (!library_)
            return;
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(library_));
#else
        dlclose(library_);
#endif
        library_ = nullptr;
    }

  public:
#define DECLARE(name) decltype(&::name) name = nullptr;
    SDR_SOAPY_SYMBOLS(DECLARE)
#undef DECLARE
    Api() {
#ifdef _WIN32
        library_ = LoadLibraryW(L"SoapySDR.dll");
#else
        library_ = dlopen("libSoapySDR.so.0.8", RTLD_NOW | RTLD_LOCAL);
#endif
        if (!library_)
            throw std::runtime_error("SoapySDR 0.8 runtime is not installed or discoverable");
        try {
#define LOAD(name) name = reinterpret_cast<decltype(name)>(symbol(#name));
            SDR_SOAPY_SYMBOLS(LOAD)
#undef LOAD
        } catch (...) {
            close();
            throw;
        }
    }
    ~Api() { close(); }
    Api(const Api &) = delete;
    Api &operator=(const Api &) = delete;
};
inline Api &api() {
    static Api instance;
    return instance;
}
#undef SDR_SOAPY_SYMBOLS
} // namespace soapy
