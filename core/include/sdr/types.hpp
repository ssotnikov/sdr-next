#pragma once
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace sdr {
using IQ = std::complex<float>;
inline constexpr std::size_t block_size = 16384;
struct Config {
    double sample_rate = 2400000;
    double center_hz = 100000000;
    double offset_hz = 0;
    double gain = 20;
    double ppm = 0;
    bool bias_t = false;
    int direct_sampling = 0;
    std::string mode = "WFM";
    double squelch_db = -100;
    std::string file;
};
struct Block {
    std::vector<IQ> samples;
    std::uint64_t sample_index = 0;
    std::uint64_t sequence = 0;
    bool discontinuity = false;
};
struct DeviceInfo {
    std::string id;
    std::string label;
    std::vector<double> rates;
    double min_hz = 0, max_hz = 0, min_gain = 0, max_gain = 0;
    bool gain = false, ppm = false, bias_t = false, direct_sampling = false;
};
inline constexpr unsigned audio_rate = 48000;
} // namespace sdr
