#pragma once
#include "types.hpp"
#include <memory>

namespace sdr {
class Source {
  public:
    virtual ~Source() = default;
    // A zero-length block means timeout; false means EOF. Throws on a fatal error.
    virtual bool read(Block &) = 0;
    virtual bool paced() const { return true; }
};
std::vector<DeviceInfo> enumerate_devices();
bool hardware_available();
std::unique_ptr<Source> open_source(const std::string &id, Config &config);
} // namespace sdr
