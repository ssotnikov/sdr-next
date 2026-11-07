#include <algorithm>
#include <cmath>
#include <sdr/device.hpp>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace sdr {
namespace {
void close_library(void *library) {
    if (!library)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}
template <std::size_t N> bool terminated(const char (&text)[N]) {
    return std::find(text, text + N, '\0') != text + N;
}
bool valid(const sdr_device_config &c) {
    return c.struct_size >= sizeof(c) && std::isfinite(c.sample_rate) && c.sample_rate >= 48000 &&
           c.sample_rate <= 20000000 && std::isfinite(c.center_hz) && c.center_hz >= 0 &&
           std::isfinite(c.gain) && std::isfinite(c.ppm) && c.bias_t <= 1 &&
           c.direct_sampling >= 0 && c.direct_sampling <= 2;
}
} // namespace
DeviceModule::DeviceModule(const std::filesystem::path &path, const std::string &expected_id) {
#ifdef _WIN32
    library_ = LoadLibraryExW(std::filesystem::absolute(path).c_str(), nullptr,
                              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto init = library_ ? reinterpret_cast<sdr_device_plugin_init_fn>(GetProcAddress(
                                     static_cast<HMODULE>(library_), "sdr_device_plugin_init"))
                               : nullptr;
#else
    library_ = dlopen(std::filesystem::absolute(path).c_str(), RTLD_NOW | RTLD_LOCAL);
    const auto init =
        library_
            ? reinterpret_cast<sdr_device_plugin_init_fn>(dlsym(library_, "sdr_device_plugin_init"))
            : nullptr;
#endif
    try {
        api_.struct_size = sizeof(api_);
        if (!init || init(SDR_DEVICE_ABI_MAJOR, SDR_DEVICE_ABI_MINOR, &api_) != SDR_OK ||
            api_.struct_size < sizeof(api_) || api_.abi_major != SDR_DEVICE_ABI_MAJOR ||
            api_.abi_minor > SDR_DEVICE_ABI_MINOR || !api_.id || !api_.id[0] ||
            (!expected_id.empty() && expected_id != api_.id) || !api_.enumerate || !api_.create ||
            !api_.destroy || !api_.read || !api_.last_error)
            throw std::runtime_error(
                "Device module rejected: library, entrypoint, ABI or identity mismatch");
    } catch (...) {
        close_library(library_);
        library_ = nullptr;
        throw;
    }
}
DeviceModule::~DeviceModule() {
    close_library(library_);
}
std::string DeviceModule::error() const {
    const char *message = api_.last_error();
    if (!message)
        return "Device module failed";
    std::size_t length = 0;
    while (length < 511 && message[length])
        ++length;
    return std::string(message, length);
}
std::vector<DeviceInfo> DeviceModule::devices() const {
    std::vector<DeviceInfo> result;
    for (uint32_t i = 0; i <= 256; ++i) {
        sdr_device_info d{};
        d.struct_size = sizeof(d);
        const auto rc = api_.enumerate(i, &d);
        if (rc == SDR_EOF)
            return result;
        if (rc != SDR_OK)
            throw std::runtime_error(error());
        if (i == 256 || d.struct_size < sizeof(d) || !terminated(d.key) || !terminated(d.label) ||
            !d.key[0] || !d.label[0] || d.rate_count > SDR_DEVICE_MAX_RATES ||
            (d.capabilities & ~15u) || !std::isfinite(d.min_hz) || !std::isfinite(d.max_hz) ||
            d.min_hz < 0 || d.max_hz < d.min_hz || !std::isfinite(d.min_gain) ||
            !std::isfinite(d.max_gain) || d.max_gain < d.min_gain)
            throw std::runtime_error("Invalid device descriptor or enumeration limit");
        DeviceInfo info;
        info.id = d.key;
        info.label = d.label;
        for (uint32_t n = 0; n < d.rate_count; ++n) {
            if (!std::isfinite(d.rates[n]) || d.rates[n] <= 0)
                throw std::runtime_error("Invalid device sample rate");
            info.rates.push_back(d.rates[n]);
        }
        if (std::any_of(result.begin(), result.end(),
                        [&](const auto &entry) { return entry.id == info.id; }))
            throw std::runtime_error("Duplicate device key");
        info.min_hz = d.min_hz;
        info.max_hz = d.max_hz;
        info.min_gain = d.min_gain;
        info.max_gain = d.max_gain;
        info.gain = d.capabilities & SDR_DEVICE_GAIN;
        info.ppm = d.capabilities & SDR_DEVICE_PPM;
        info.bias_t = d.capabilities & SDR_DEVICE_BIAS_T;
        info.direct_sampling = d.capabilities & SDR_DEVICE_DIRECT_SAMPLING;
        result.push_back(std::move(info));
    }
    throw std::runtime_error("Device enumeration did not terminate");
}
DeviceSource::DeviceSource(std::shared_ptr<DeviceModule> module, const std::string &key, Config &c)
    : module_(std::move(module)), scratch_(block_size) {
    const sdr_device_config requested{sizeof(sdr_device_config),
                                      c.bias_t ? 1u : 0u,
                                      c.direct_sampling,
                                      c.sample_rate,
                                      c.center_hz,
                                      c.gain,
                                      c.ppm};
    if (!valid(requested))
        throw std::invalid_argument("Invalid device configuration");
    auto actual = requested;
    instance_ = module_->api().create(key.c_str(), &requested, &actual);
    if (!instance_)
        throw std::runtime_error(module_->error());
    if (!valid(actual)) {
        module_->api().destroy(instance_);
        instance_ = nullptr;
        throw std::runtime_error("Device module returned invalid actual parameters");
    }
    c.sample_rate = actual.sample_rate;
    c.center_hz = actual.center_hz;
    c.gain = actual.gain;
    c.ppm = actual.ppm;
    c.bias_t = actual.bias_t != 0;
    c.direct_sampling = actual.direct_sampling;
}
DeviceSource::~DeviceSource() {
    if (instance_)
        module_->api().destroy(instance_);
}
bool DeviceSource::read(Block &b) {
    uint32_t count = 0, flags = 0;
    const auto result = module_->api().read(instance_, scratch_.data(),
                                            static_cast<uint32_t>(scratch_.size()), &count, &flags);
    if (result == SDR_EOF)
        return false;
    if (result != SDR_OK)
        throw std::runtime_error(module_->error());
    if (count > scratch_.size() || (flags & ~SDR_DEVICE_DISCONTINUITY))
        throw std::runtime_error("Device module exceeded IQ capacity or returned invalid flags");
    b.samples.resize(count);
    b.sample_index = index_;
    b.sequence = sequence_++;
    b.discontinuity = (flags & SDR_DEVICE_DISCONTINUITY) != 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(scratch_[i].i) || !std::isfinite(scratch_[i].q))
            throw std::runtime_error("Device module returned non-finite IQ");
        b.samples[i] = {scratch_[i].i, scratch_[i].q};
    }
    index_ += count;
    return true;
}
} // namespace sdr
