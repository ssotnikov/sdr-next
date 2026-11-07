#pragma once
#include "source.hpp"
#include <filesystem>
#include <sdr/device_plugin.h>

namespace sdr {
class DeviceModule {
  public:
    DeviceModule(const std::filesystem::path &, const std::string &expected_id = {});
    ~DeviceModule();
    DeviceModule(const DeviceModule &) = delete;
    DeviceModule &operator=(const DeviceModule &) = delete;
    const sdr_device_api &api() const { return api_; }
    std::vector<DeviceInfo> devices() const;
    std::string error() const;

  private:
    void *library_ = nullptr;
    sdr_device_api api_{};
};
class DeviceSource final : public Source {
  public:
    DeviceSource(std::shared_ptr<DeviceModule>, const std::string &key, Config &actual);
    ~DeviceSource() override;
    DeviceSource(const DeviceSource &) = delete;
    DeviceSource &operator=(const DeviceSource &) = delete;
    bool read(Block &) override;
    bool paced() const override { return false; }

  private:
    std::shared_ptr<DeviceModule> module_;
    void *instance_ = nullptr;
    std::vector<sdr_iq> scratch_;
    std::uint64_t index_ = 0, sequence_ = 0;
};
} // namespace sdr
