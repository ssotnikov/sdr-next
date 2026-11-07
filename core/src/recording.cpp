#include <algorithm>
#include <bit>
#include <cmath>
#include <sdr/recording.hpp>
#include <stdexcept>

namespace sdr {
namespace {
void u16(std::ostream &out, std::uint16_t value) {
    const char b[] = {static_cast<char>(value), static_cast<char>(value >> 8)};
    out.write(b, 2);
}
void u32(std::ostream &out, std::uint32_t value) {
    const char b[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                      static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    out.write(b, 4);
}
void header(std::ostream &s, std::uint32_t bytes) {
    s.write("RIFF", 4);
    u32(s, 36 + bytes);
    s.write("WAVEfmt ", 8);
    u32(s, 16);
    u16(s, 1);
    u16(s, 1);
    u32(s, audio_rate);
    u32(s, audio_rate * 2);
    u16(s, 2);
    u16(s, 16);
    s.write("data", 4);
    u32(s, bytes);
}
} // namespace
WavWriter::~WavWriter() {
    try {
        close();
    } catch (...) {
    }
}
void WavWriter::open(const std::filesystem::path &path) {
    close();
    samples_ = 0;
    stream_.open(path, std::ios::binary | std::ios::trunc);
    if (!stream_)
        throw std::runtime_error("Cannot create WAV file");
    header(stream_, 0);
    if (!stream_)
        throw std::runtime_error("Cannot write WAV header");
}
void WavWriter::write(std::span<const float> data) {
    if (!stream_.is_open())
        throw std::logic_error("WAV recorder is closed");
    if (samples_ + data.size() > (0xffffffffULL - 36) / 2)
        throw std::runtime_error("WAV size limit reached (4 GiB)");
    std::vector<std::uint16_t> bytes;
    bytes.reserve(data.size());
    for (const float x : data) {
        const auto v = static_cast<std::int16_t>(
            std::lround(std::clamp(std::isfinite(x) ? x : 0.f, -1.f, 1.f) * 32767));
        auto word = static_cast<std::uint16_t>(v);
        if constexpr (std::endian::native == std::endian::big)
            word = static_cast<std::uint16_t>((word >> 8) | (word << 8));
        bytes.push_back(word);
    }
    stream_.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size() * 2));
    if (!stream_)
        throw std::runtime_error("WAV write failed (disk full or unavailable)");
    samples_ += data.size();
}
void WavWriter::close() {
    if (!stream_.is_open())
        return;
    bool failed = !stream_;
    if (!failed) {
        stream_.seekp(0);
        header(stream_, static_cast<std::uint32_t>(samples_ * 2));
        stream_.flush();
        failed = !stream_;
    }
    stream_.close();
    if (failed || stream_.fail())
        throw std::runtime_error("WAV recording is incomplete");
}
void IqWriter::open(const std::filesystem::path &path) {
    close();
    samples_ = 0;
    stream_.open(path, std::ios::binary | std::ios::trunc);
    if (!stream_)
        throw std::runtime_error("Cannot create IQ file");
}
void IqWriter::write(std::span<const IQ> data) {
    if (!stream_.is_open())
        throw std::logic_error("IQ recorder is closed");
    if constexpr (std::endian::native == std::endian::little) {
        static_assert(sizeof(IQ) == 8);
        stream_.write(reinterpret_cast<const char *>(data.data()),
                      static_cast<std::streamsize>(data.size_bytes()));
    } else
        for (const auto x : data) {
            u32(stream_, std::bit_cast<std::uint32_t>(x.real()));
            u32(stream_, std::bit_cast<std::uint32_t>(x.imag()));
        }
    if (!stream_)
        throw std::runtime_error("IQ write failed (disk full or unavailable)");
    samples_ += data.size();
}
void IqWriter::close() {
    if (!stream_.is_open())
        return;
    stream_.flush();
    bool failed = !stream_;
    stream_.close();
    if (failed || stream_.fail())
        throw std::runtime_error("IQ recording is incomplete");
}
} // namespace sdr
