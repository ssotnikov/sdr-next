#include <algorithm>
#include <cmath>
#include <sdr/plugin.hpp>
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
void unload(void *library) {
    if (!library)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}
} // namespace
PluginModule::PluginModule(const std::filesystem::path &path, const std::string &expected_id,
                           unsigned kind) {
#ifdef _WIN32
    library_ = LoadLibraryExW(std::filesystem::absolute(path).c_str(), nullptr,
                              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto init = library_ ? reinterpret_cast<sdr_plugin_init_fn>(GetProcAddress(
                                     static_cast<HMODULE>(library_), "sdr_plugin_init"))
                               : nullptr;
#else
    library_ = dlopen(std::filesystem::absolute(path).c_str(), RTLD_NOW | RTLD_LOCAL);
    const auto init = library_
                          ? reinterpret_cast<sdr_plugin_init_fn>(dlsym(library_, "sdr_plugin_init"))
                          : nullptr;
#endif
    api_.struct_size = sizeof(api_);
    if (!init || init(SDR_ABI_MAJOR, SDR_ABI_MINOR, &api_) != SDR_OK ||
        api_.struct_size < sizeof(api_) || api_.abi_major != SDR_ABI_MAJOR ||
        api_.abi_minor > SDR_ABI_MINOR || api_.kind != kind || !api_.id || expected_id != api_.id ||
        !api_.create || !api_.destroy || !api_.configure || (kind == SDR_SOURCE && !api_.read) ||
        (kind == SDR_DEMODULATOR && !api_.process) ||
        (kind != SDR_SOURCE && kind != SDR_DEMODULATOR)) {
        unload(library_);
        library_ = nullptr;
        throw std::runtime_error(
            "Plugin rejected: library, entrypoint, ABI, kind or identity mismatch");
    }
}
PluginModule::~PluginModule() {
    unload(library_);
}
PluginInstance::PluginInstance(std::shared_ptr<PluginModule> module, const Config &config)
    : module_(std::move(module)) {
    object_ = module_->api().create();
    if (!object_)
        throw std::runtime_error("Plugin create failed");
    const sdr_config c{sizeof(sdr_config), audio_rate, config.sample_rate, config.center_hz,
                       config.offset_hz};
    if (module_->api().configure(object_, &c) != SDR_OK) {
        module_->api().destroy(object_);
        object_ = nullptr;
        throw std::runtime_error("Plugin configuration rejected");
    }
    scratch_.resize(block_size);
}
PluginInstance::~PluginInstance() {
    if (object_)
        module_->api().destroy(object_);
}
bool PluginInstance::read(Block &block) {
    uint32_t count = 0, flags = 0;
    const int result = module_->api().read(object_, scratch_.data(),
                                           static_cast<uint32_t>(scratch_.size()), &count, &flags);
    if (result == SDR_EOF)
        return false;
    if (result != SDR_OK || count > scratch_.size())
        throw std::runtime_error("Plugin read failed or exceeded capacity");
    block.samples.resize(count);
    block.sample_index = index_;
    block.sequence = sequence_++;
    block.discontinuity = (flags & 1) != 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(scratch_[i].i) || !std::isfinite(scratch_[i].q))
            throw std::runtime_error("Plugin returned non-finite IQ");
        block.samples[i] = {scratch_[i].i, scratch_[i].q};
    }
    index_ += count;
    return true;
}
void PluginInstance::process(std::span<const IQ> input, std::vector<float> &output) {
    if (input.size() > block_size)
        throw std::runtime_error("Plugin input block exceeds capacity");
    for (std::size_t i = 0; i < input.size(); ++i)
        scratch_[i] = {input[i].real(), input[i].imag()};
    output.resize(block_size);
    uint32_t count = 0;
    if (module_->api().process(object_, scratch_.data(), static_cast<uint32_t>(input.size()),
                               output.data(), static_cast<uint32_t>(output.size()),
                               &count) != SDR_OK ||
        count > output.size())
        throw std::runtime_error("Plugin process failed or exceeded capacity");
    output.resize(count);
    for (const float x : output)
        if (!std::isfinite(x))
            throw std::runtime_error("Plugin returned non-finite audio");
}
} // namespace sdr
