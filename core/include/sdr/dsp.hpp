#pragma once
#include "types.hpp"
#include <span>

namespace sdr {
class Demodulator {
  public:
    void configure(const Config &);
    void process(std::span<const IQ> input, std::vector<float> &output);
    double power_db() const { return power_db_; }

  private:
    Config config_;
    std::vector<float> channel_taps_, audio_taps_;
    std::vector<IQ> channel_ring_;
    std::vector<float> audio_ring_;
    std::size_t channel_pos_ = 0, audio_pos_ = 0;
    unsigned decimation_ = 1, phase_ = 0;
    double channel_rate_ = 48000, audio_phase_ = 0;
    IQ oscillator_{1, 0}, rotation_{1, 0}, previous_{1, 0};
    double dc_ = 0, amplitude_ = 0.01, deemphasis_ = 0, power_db_ = -120;
};
std::vector<float> spectrum(std::span<const IQ> samples, std::size_t size = 2048);
} // namespace sdr
