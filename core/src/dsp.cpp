#include <algorithm>
#include <cmath>
#include <numbers>
#include <sdr/dsp.hpp>
#include <stdexcept>

namespace sdr {
namespace {
constexpr double pi = std::numbers::pi;
std::vector<float> lowpass(std::size_t count, double cutoff, double rate) {
    std::vector<float> taps(count);
    double sum = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const double x = static_cast<double>(i) - static_cast<double>(count - 1) / 2;
        const double h =
            x == 0 ? 2 * cutoff / rate : std::sin(2 * pi * cutoff * x / rate) / (pi * x);
        taps[i] = static_cast<float>(h * (0.54 - 0.46 * std::cos(2 * pi * i / (count - 1))));
        sum += taps[i];
    }
    for (auto &t : taps)
        t = static_cast<float>(t / sum);
    return taps;
}
} // namespace
void Demodulator::configure(const Config &c) {
    if (!std::isfinite(c.sample_rate) || c.sample_rate < 48000 || c.sample_rate > 20000000 ||
        !std::isfinite(c.offset_hz) || std::abs(c.offset_hz) >= c.sample_rate / 2)
        throw std::invalid_argument("Invalid sample rate or channel offset");
    if (c.mode != "AM" && c.mode != "NFM" && c.mode != "WFM")
        throw std::invalid_argument("Unsupported demodulator");
    if (c.mode == "WFM" && c.sample_rate < 192000)
        throw std::invalid_argument("WFM requires at least 192 kSPS");
    config_ = c;
    const double desired = c.mode == "WFM" ? 192000 : 48000;
    decimation_ = std::max(1u, static_cast<unsigned>(c.sample_rate / desired));
    channel_rate_ = c.sample_rate / decimation_;
    const double cutoff = c.mode == "WFM" ? 85000 : c.mode == "NFM" ? 10000 : 6000;
    channel_taps_ =
        lowpass(16 * decimation_ + 1, std::min(cutoff, channel_rate_ * 0.45), c.sample_rate);
    channel_ring_.assign(channel_taps_.size(), {});
    audio_taps_ = lowpass(63, c.mode == "WFM" ? 15000 : 5000, channel_rate_);
    audio_ring_.assign(audio_taps_.size(), 0);
    channel_pos_ = audio_pos_ = phase_ = 0;
    audio_phase_ = 0;
    oscillator_ = previous_ = {1, 0};
    rotation_ = std::polar(1.0f, static_cast<float>(-2 * pi * c.offset_hz / c.sample_rate));
    dc_ = 0;
    amplitude_ = 0.01;
    deemphasis_ = 0;
    power_db_ = -120;
}
void Demodulator::process(std::span<const IQ> input, std::vector<float> &output) {
    if (channel_ring_.empty())
        throw std::logic_error("Demodulator is not prepared");
    output.clear();
    double energy = 0;
    for (const auto sample : input) {
        energy += std::norm(sample);
        channel_ring_[channel_pos_] = sample * oscillator_;
        oscillator_ *= rotation_;
        channel_pos_ = (channel_pos_ + 1) % channel_ring_.size();
        if (++phase_ < decimation_)
            continue;
        phase_ = 0;
        IQ filtered{};
        std::size_t pos = channel_pos_;
        for (std::size_t j = 0; j < channel_taps_.size(); ++j) {
            pos = pos ? pos - 1 : channel_ring_.size() - 1;
            filtered += channel_ring_[pos] * channel_taps_[j];
        }
        double value;
        if (config_.mode == "AM") {
            const double envelope = std::abs(filtered);
            dc_ += 0.002 * (envelope - dc_);
            amplitude_ += 0.0005 * (envelope - amplitude_);
            value = (envelope - dc_) / std::max(0.05, amplitude_) * 0.5;
        } else {
            const IQ difference = filtered * std::conj(previous_);
            previous_ = filtered;
            value = std::atan2(difference.imag(), difference.real()) * channel_rate_ /
                    (2 * pi * (config_.mode == "WFM" ? 75000 : 5000));
            if (config_.mode == "WFM") {
                const double a = 1.0 / (1.0 + channel_rate_ * 75e-6);
                deemphasis_ += a * (value - deemphasis_);
                value = deemphasis_;
            }
        }
        audio_ring_[audio_pos_] = static_cast<float>(value);
        audio_pos_ = (audio_pos_ + 1) % audio_ring_.size();
        audio_phase_ += audio_rate / channel_rate_;
        if (audio_phase_ < 1)
            continue;
        audio_phase_ -= 1;
        double audio = 0;
        pos = audio_pos_;
        for (std::size_t j = 0; j < audio_taps_.size(); ++j) {
            pos = pos ? pos - 1 : audio_ring_.size() - 1;
            audio += audio_ring_[pos] * audio_taps_[j];
        }
        output.push_back(static_cast<float>(std::clamp(audio, -1.0, 1.0)));
    }
    const float magnitude = std::abs(oscillator_);
    if (magnitude > 0)
        oscillator_ /= magnitude;
    if (!input.empty())
        power_db_ = 10 * std::log10(std::max(energy / input.size(), 1e-12));
    if (power_db_ < config_.squelch_db)
        std::fill(output.begin(), output.end(), 0);
}
std::vector<float> spectrum(std::span<const IQ> samples, std::size_t n) {
    if (n < 2 || (n & (n - 1)))
        throw std::invalid_argument("FFT size must be a power of two");
    std::vector<IQ> data(n);
    double window_sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * pi * i / (n - 1));
        window_sum += w;
        if (i < samples.size())
            data[i] = samples[i] * static_cast<float>(w);
    }
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const IQ step = std::polar(1.0f, static_cast<float>(-2 * pi / length));
        for (std::size_t i = 0; i < n; i += length) {
            IQ w{1, 0};
            for (std::size_t j = 0; j < length / 2; ++j) {
                const IQ u = data[i + j], v = data[i + j + length / 2] * w;
                data[i + j] = u + v;
                data[i + j + length / 2] = u - v;
                w *= step;
            }
        }
    }
    std::vector<float> result(n);
    for (std::size_t i = 0; i < n; ++i)
        result[i] = static_cast<float>(
            10 * std::log10(std::max(static_cast<double>(std::norm(data[(i + n / 2) % n])) /
                                         (window_sum * window_sum),
                                     1e-12)));
    return result;
}
} // namespace sdr
