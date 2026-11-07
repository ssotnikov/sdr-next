#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sdr/path.hpp>
#include <sdr/source.hpp>
#include <stdexcept>

namespace sdr {
namespace {
class Synthetic final : public Source {
    Config config_;
    double phase_ = 0;
    std::uint64_t index_ = 0, sequence_ = 0;

  public:
    explicit Synthetic(Config c) : config_(std::move(c)) {}
    bool read(Block &b) override {
        b.samples.resize(block_size);
        b.sample_index = index_;
        b.sequence = sequence_++;
        b.discontinuity = false;
        for (auto &s : b.samples) {
            const double t = static_cast<double>(index_++) / config_.sample_rate;
            const double tone = std::sin(2 * std::numbers::pi * 1000 * t);
            if (config_.mode == "AM") {
                s = std::polar(static_cast<float>(0.5 * (1 + 0.5 * tone)),
                               static_cast<float>(2 * std::numbers::pi * config_.offset_hz * t));
            } else {
                phase_ += 2 * std::numbers::pi *
                          (config_.offset_hz + (config_.mode == "WFM" ? 50000 : 2500) * tone) /
                          config_.sample_rate;
                phase_ = std::remainder(phase_, 2 * std::numbers::pi);
                s = std::polar(0.5f, static_cast<float>(phase_));
            }
        }
        return true;
    }
};
float read_float(const char *bytes) {
    std::uint32_t u = 0;
    for (unsigned j = 0; j < 4; ++j)
        u |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[j])) << (8 * j);
    return std::bit_cast<float>(u);
}
class File final : public Source {
    std::ifstream stream_;
    std::vector<char> buffer_ = std::vector<char>(block_size * 8);
    std::uint64_t index_ = 0, sequence_ = 0;

  public:
    explicit File(const std::string &path) : stream_(path_from_utf8(path), std::ios::binary) {
        if (!stream_)
            throw std::runtime_error("Cannot open IQ file");
    }
    bool read(Block &b) override {
        stream_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        const auto bytes = static_cast<std::size_t>(stream_.gcount());
        if (stream_.bad())
            throw std::runtime_error("IQ file read error");
        if (!bytes)
            return false;
        if (bytes % 8)
            throw std::runtime_error("Truncated CF32 IQ sample");
        b.sample_index = index_;
        b.sequence = sequence_++;
        b.discontinuity = false;
        b.samples.resize(bytes / 8);
        for (std::size_t i = 0; i < b.samples.size(); ++i) {
            const float re = read_float(buffer_.data() + i * 8),
                        im = read_float(buffer_.data() + i * 8 + 4);
            if (!std::isfinite(re) || !std::isfinite(im))
                throw std::runtime_error("Non-finite IQ sample");
            b.samples[i] = {re, im};
        }
        index_ += b.samples.size();
        return true;
    }
};
} // namespace
std::vector<DeviceInfo> enumerate_devices() {
    std::vector<DeviceInfo> devices;
    devices.push_back({"synthetic",
                       "Synthetic / 1 kHz",
                       {2400000, 10000000, 20000000},
                       0,
                       6000000000,
                       0,
                       0,
                       false,
                       false,
                       false,
                       false});
    devices.push_back({"file",
                       "Raw IQ / CF32 little-endian",
                       {},
                       0,
                       6000000000,
                       0,
                       0,
                       false,
                       false,
                       false,
                       false});

    return devices;
}
std::unique_ptr<Source> open_source(const std::string &id, Config &c) {
    if (!std::isfinite(c.sample_rate) || c.sample_rate < 48000 || c.sample_rate > 20000000 ||
        !std::isfinite(c.center_hz) || c.center_hz < 0 || !std::isfinite(c.gain) ||
        !std::isfinite(c.ppm))
        throw std::invalid_argument("Invalid source configuration");
    if (id == "synthetic")
        return std::make_unique<Synthetic>(c);
    if (id == "file")
        return std::make_unique<File>(c.file);

    throw std::invalid_argument("Unknown built-in source");
}
} // namespace sdr
