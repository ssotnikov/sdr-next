#pragma once
#include "types.hpp"
#include <filesystem>
#include <fstream>
#include <span>

namespace sdr {
class WavWriter {
  public:
    ~WavWriter();
    void open(const std::filesystem::path &);
    void write(std::span<const float>);
    void close();
    std::uint64_t samples() const { return samples_; }

  private:
    std::ofstream stream_;
    std::uint64_t samples_ = 0;
};
class IqWriter {
  public:
    void open(const std::filesystem::path &);
    void write(std::span<const IQ>);
    void close();
    std::uint64_t samples() const { return samples_; }

  private:
    std::ofstream stream_;
    std::uint64_t samples_ = 0;
};
} // namespace sdr
