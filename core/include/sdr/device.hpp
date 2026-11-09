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
    sdr_device_read_diagnostics_v1_fn diagnostic_read() const { return diagnostic_read_; }
    std::vector<DeviceInfo> devices() const;
    std::string error() const;

  private:
    void *library_ = nullptr;
    sdr_device_api api_{};
    sdr_device_read_diagnostics_v1_fn diagnostic_read_ = nullptr;
};
struct DeviceReadTrace {
    sdr_device_read_metrics_v1 driver{-1, 0};
    std::int64_t conversion_us = -1;
};

class DeviceSource final : public Source {
  public:
    DeviceSource(std::shared_ptr<DeviceModule>, const std::string &key, Config &actual);
    ~DeviceSource() override;
    DeviceSource(const DeviceSource &) = delete;
    DeviceSource &operator=(const DeviceSource &) = delete;
    bool read(Block &) override;
    bool read(Block &, DeviceReadTrace *);
    bool paced() const override { return false; }

  private:
    std::shared_ptr<DeviceModule> module_;
    void *instance_ = nullptr;
    std::vector<sdr_iq> scratch_;
    std::uint64_t index_ = 0, sequence_ = 0;
};
} // namespace sdr
