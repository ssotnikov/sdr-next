#pragma once
#include "source.hpp"
#include <filesystem>
#include <sdr/plugin.h>
#include <span>

namespace sdr {
class PluginModule {
  public:
    PluginModule(const std::filesystem::path &, const std::string &expected_id,
                 unsigned expected_kind);
    ~PluginModule();
    PluginModule(const PluginModule &) = delete;
    PluginModule &operator=(const PluginModule &) = delete;
    const sdr_plugin_api &api() const { return api_; }

  private:
    void *library_ = nullptr;
    sdr_plugin_api api_{};
};
class PluginInstance {
  public:
    PluginInstance(std::shared_ptr<PluginModule>, const Config &);
    ~PluginInstance();
    PluginInstance(const PluginInstance &) = delete;
    PluginInstance &operator=(const PluginInstance &) = delete;
    bool read(Block &);
    void process(std::span<const IQ>, std::vector<float> &);

  private:
    std::shared_ptr<PluginModule> module_;
    void *object_ = nullptr;
    std::vector<sdr_iq> scratch_;
    std::uint64_t index_ = 0, sequence_ = 0;
};
} // namespace sdr
