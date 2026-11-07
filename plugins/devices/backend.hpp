#pragma once
#include <memory>
#include <sdr/device_plugin.h>
#include <string>
#include <vector>
namespace device_backend {
class Receiver {
  public:
    virtual ~Receiver() = default;
    virtual void read(sdr_iq *, uint32_t capacity, uint32_t &count, uint32_t &flags) = 0;
};
std::vector<sdr_device_info> enumerate(const std::string &family);
std::unique_ptr<Receiver> open(const std::string &key, sdr_device_config &actual);
} // namespace device_backend
