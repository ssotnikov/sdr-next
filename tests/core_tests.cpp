#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <sdr/bounded_queue.hpp>
#include <sdr/dsp.hpp>
#include <sdr/plugin.hpp>
#include <sdr/recording.hpp>
#include <sdr/source.hpp>
#include <stdexcept>
#include <thread>

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, "Expected rejection");
}
double correlation(const std::vector<float> &data, double hz) {
    double re = 0, im = 0;
    for (std::size_t i = 2400; i < data.size(); ++i) {
        const double phase = 2 * std::numbers::pi * hz * i / 48000;
        re += data[i] * std::cos(phase);
        im += data[i] * std::sin(phase);
    }
    return std::hypot(re, im) / data.size();
}
double tone_fit(const std::vector<float> &data, double hz) {
    double energy = 0, ss = 0, cc = 0, sc = 0, xs = 0, xc = 0;
    for (std::size_t i = 2400; i < data.size(); ++i) {
        const double phase = 2 * std::numbers::pi * hz * i / 48000;
        const double s = std::sin(phase), c = std::cos(phase), x = data[i];
        energy += x * x;
        ss += s * s;
        cc += c * c;
        sc += s * c;
        xs += x * s;
        xc += x * c;
    }
    const double determinant = ss * cc - sc * sc;
    const double a = (xs * cc - xc * sc) / determinant;
    const double b = (xc * ss - xs * sc) / determinant;
    return std::sqrt(std::clamp((a * xs + b * xc) / energy, 0.0, 1.0));
}
} // namespace
int main(int argc, char **argv) {
    try {
        sdr::BoundedQueue<int> queue(2);
        check(queue.push(1) && queue.push(2) && !queue.push(3), "Queue bound");
        check(queue.pop() == 1 && queue.pop() == 2 && !queue.pop(), "Queue order");
        sdr::BoundedQueue<int> burst(1);
        std::stop_source cancellation;
        burst.push(1);
        check(!burst.push_for(2, std::chrono::milliseconds(2), cancellation.get_token()),
              "Bounded wait times out");
        std::thread consumer([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            burst.pop();
        });
        const bool accepted =
            burst.push_for(2, std::chrono::milliseconds(500), cancellation.get_token());
        consumer.join();
        check(accepted && burst.pop() == 2, "Burst absorbs temporary consumer scheduling delay");
        cancellation.request_stop();
        check(!burst.push_for(3, std::chrono::milliseconds(500), cancellation.get_token()),
              "Stop interrupts producer wait");
        sdr::BoundedQueue<sdr::Block> iq_burst(8);
        std::size_t depth = 0;
        // Eight 16K blocks can arrive before the paused consumer runs again.
        for (std::uint64_t sequence = 0; sequence < 8; ++sequence) {
            sdr::Block block;
            block.sequence = sequence;
            block.sample_index = sequence * sdr::block_size;
            block.samples.resize(sdr::block_size, {static_cast<float>(sequence), 0});
            check(iq_burst.push_for(std::move(block), std::chrono::milliseconds(20), {}, &depth) &&
                      depth == sequence + 1,
                  "IQ burst fills the expected number of slots");
        }
        check(!iq_burst.push_for({}, std::chrono::milliseconds(20), {}, &depth) && depth == 8,
              "IQ overflow reports full depth without replacing a queued block");
        for (std::uint64_t sequence = 0; sequence < 8; ++sequence) {
            const auto block = iq_burst.pop(&depth);
            check(block && block->sequence == sequence && depth == 7 - sequence &&
                      block->sample_index == sequence * sdr::block_size &&
                      block->samples.size() == sdr::block_size &&
                      block->samples.back().real() == static_cast<float>(sequence),
                  "IQ burst drains in sample order with original payloads");
        }
        sdr::BoundedQueue<int> transport(8);
        std::thread producer([&] {
            for (int i = 0; i < 10000; ++i)
                while (!transport.push(i))
                    std::this_thread::yield();
        });
        for (int i = 0; i < 10000; ++i) {
            std::optional<int> n;
            while (!(n = transport.pop()))
                std::this_thread::yield();
            check(*n == i, "Threaded transport order");
        }
        producer.join();
        std::vector<sdr::IQ> tone(2048);
        for (std::size_t i = 0; i < tone.size(); ++i)
            tone[i] =
                std::polar(0.5f, static_cast<float>(2 * std::numbers::pi * 100 * i / tone.size()));
        const auto fft = sdr::spectrum(tone);
        const auto peak = std::max_element(fft.begin(), fft.end());
        check(peak - fft.begin() == 1124, "FFT frequency");
        check(std::abs(*peak + 6.0206) < 0.05, "FFT dBFS normalization");
        rejects([&] { sdr::spectrum(tone, 100); });
        for (double rate : {240000.0, 1000000.0, 2400000.0}) {
            for (const std::string mode : {"AM", "NFM", "WFM"}) {
                sdr::Config config;
                config.sample_rate = rate;
                config.offset_hz = 12000;
                config.mode = mode;
                sdr::Demodulator demod;
                demod.configure(config);
                auto source = sdr::open_source("synthetic", config);
                std::vector<float> audio, part;
                sdr::Block block;
                for (int i = 0; i < std::ceil(rate * 0.5 / sdr::block_size); ++i) {
                    source->read(block);
                    demod.process(block.samples, part);
                    audio.insert(audio.end(), part.begin(), part.end());
                }
                check(audio.size() > 24000 && audio.size() < 27000, "Audio sample rate");
                check(correlation(audio, 1000) > 0.04, "Demodulated tone present");
                check(correlation(audio, 1000) > 10 * correlation(audio, 3000),
                      "Demodulated tone selectivity");
                check(tone_fit(audio, 1000) >= 0.95, "DSP accepted tone correlation");
                int peak_hz = 980;
                for (int hz = 981; hz <= 1020; ++hz)
                    if (correlation(audio, hz) > correlation(audio, peak_hz))
                        peak_hz = hz;
                check(std::abs(peak_hz - 1000) <= 10, "DSP accepted tone frequency");
                std::cout << mode << " rate=" << rate
                          << " tone_correlation=" << tone_fit(audio, 1000)
                          << " frequency_hz=" << peak_hz << '\n';
                config.offset_hz = config.sample_rate;
                rejects([&] { demod.configure(config); });
            }
        }
        // Compare the same AM-modulated carrier before/after moving it outside the channel.
        const auto carrier_audio = [](double offset) {
            sdr::Config config;
            config.sample_rate = 2400000;
            config.mode = "AM";
            auto generated = config;
            generated.offset_hz = offset;
            auto source = sdr::open_source("synthetic", generated);
            sdr::Demodulator demod;
            demod.configure(config);
            sdr::Block block;
            std::vector<float> audio, part;
            for (int i = 0; i < 80; ++i) {
                source->read(block);
                demod.process(block.samples, part);
                audio.insert(audio.end(), part.begin(), part.end());
            }
            return correlation(audio, 1000);
        };
        const double rejection_db = 20 * std::log10(carrier_audio(200000) / carrier_audio(0));
        std::cout << "AM out_of_band_db=" << rejection_db << '\n';
        check(rejection_db <= -40, "DSP accepted out-of-band rejection");
        const auto folder =
            std::filesystem::temp_directory_path() /
            std::filesystem::path(
                "sdr-tests-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
        const auto iqpath = folder / "test.cf32";
        sdr::IqWriter iq;
        iq.open(iqpath);
        iq.write(tone);
        iq.close();
        check(std::filesystem::file_size(iqpath) == tone.size() * 8, "IQ byte count");
        sdr::Config c;
        c.file = iqpath.string();
        auto file = sdr::open_source("file", c);
        sdr::Block b;
        check(file->read(b) && b.samples == tone && !file->read(b), "IQ round trip and EOF");
        {
            std::ofstream broken(folder / "broken.cf32", std::ios::binary);
            broken.put('x');
        }
        c.file = (folder / "broken.cf32").string();
        auto broken = sdr::open_source("file", c);
        rejects([&] { broken->read(b); });
        sdr::WavWriter wav;
        wav.open(folder / "audio.wav");
        wav.write(std::vector<float>(4800, 0.25f));
        wav.close();
        check(std::filesystem::file_size(folder / "audio.wav") == 9644, "WAV byte count");
        {
            std::ifstream data(folder / "audio.wav", std::ios::binary);
            char header[44];
            data.read(header, 44);
            check(std::string(header, 4) == "RIFF" && std::string(header + 8, 4) == "WAVE" &&
                      static_cast<unsigned char>(header[34]) == 16,
                  "WAV PCM header");
        }
        rejects([&] { wav.open(folder / "absent" / "audio.wav"); });
        if (argc >= 3) {
            auto module =
                std::make_shared<sdr::PluginModule>(argv[1], "org.sdr.example.source", SDR_SOURCE);
            sdr::Config config;
            {
                sdr::PluginInstance instance(module, config);
                module.reset();
                check(instance.read(b) && b.samples.size() == sdr::block_size,
                      "Module held during instance lifetime");
                const auto expectedPhase = 2 * std::numbers::pi * 1000 / config.sample_rate;
                check(std::abs(std::arg(b.samples[1]) - expectedPhase) < 1e-6,
                      "C++ Source IQ has the expected phase increment");
            }
            rejects([&] { sdr::PluginModule invalid(argv[1], "wrong.id", SDR_SOURCE); });
            auto demod = std::make_shared<sdr::PluginModule>(argv[2], "org.sdr.example.demod",
                                                             SDR_DEMODULATOR);
            config.mode = "AM";
            sdr::PluginInstance instance(demod, config);
            auto amSource = sdr::open_source("synthetic", config);
            std::vector<float> audio, part;
            for (int block = 0; block < 16; ++block) {
                check(amSource->read(b), "AM input block available");
                instance.process(b.samples, part);
                audio.insert(audio.end(), part.begin(), part.end());
            }
            check(correlation(audio, 1000) > 0.04 && tone_fit(audio, 1000) > 0.95,
                  "AM demodulator recovers the 1 kHz modulation tone");
            auto invalidConfig = config;
            invalidConfig.offset_hz = 1000;
            rejects([&] { sdr::PluginInstance rejected(demod, invalidConfig); });
            std::cout << "C++ Source IQ and Demodulator audio passed\n";
        }
        if (argc >= 4 && std::string(argv[3]).size()) {
            auto rust = std::make_shared<sdr::PluginModule>(argv[3], "org.sdr.example.rust-source",
                                                            SDR_SOURCE);
            sdr::Config config;
            auto invalidConfig = config;
            invalidConfig.sample_rate = 1;
            rejects([&] { sdr::PluginInstance rejected(rust, invalidConfig); });
            const std::weak_ptr<sdr::PluginModule> lifetime = rust;
            {
                sdr::PluginInstance instance(rust, config);
                rust.reset();
                check(!lifetime.expired() && instance.read(b) && b.samples.size() == sdr::block_size,
                      "Rust source called from C++ host with module held by instance");
                const auto expectedPhase = 2 * std::numbers::pi * 1000 / config.sample_rate;
                check(std::abs(std::arg(b.samples[1]) - expectedPhase) < 1e-6,
                      "Rust Source IQ has the expected phase increment");
            }
            check(lifetime.expired(), "Rust module released after destroying the instance");
            std::cout << "Rust Source FFI, configuration rejection and teardown passed\n";
        }
        for (int i = 4; i < argc; ++i)
            rejects([&] { sdr::PluginModule invalid(argv[i], "org.sdr.example.bad", SDR_SOURCE); });
        file.reset();
        broken.reset();
        std::filesystem::remove_all(folder);
        std::cout << "Core tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
