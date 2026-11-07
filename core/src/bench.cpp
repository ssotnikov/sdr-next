#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sdr/device.hpp>
#include <sdr/dsp.hpp>
#include <sdr/path.hpp>
#include <sdr/recording.hpp>
#include <sdr/source.hpp>
#include <stdexcept>

int main(int argc, char **argv) {
    try {
        sdr::Config config;
        double seconds = 1;
        std::string output, module_path, device = "synthetic";
        for (int i = 1; i < argc; ++i) {
            const std::string flag = argv[i];
            if (i + 1 >= argc)
                throw std::invalid_argument("Expected value after argument");
            const std::string value = argv[++i];
            if (flag == "--rate")
                config.sample_rate = std::stod(value);
            else if (flag == "--seconds")
                seconds = std::stod(value);
            else if (flag == "--mode")
                config.mode = value;
            else if (flag == "--write-iq")
                output = value;
            else if (flag == "--device")
                device = value;
            else if (flag == "--module")
                module_path = value;
            else
                throw std::invalid_argument("Unknown argument");
        }
        if (!std::isfinite(seconds) || seconds <= 0 || seconds > 28800)
            throw std::invalid_argument("Duration must be 0 < seconds <= 28800");
        std::unique_ptr<sdr::Source> source;
        if (module_path.empty())
            source = sdr::open_source(device, config);
        else {
            auto module = std::make_shared<sdr::DeviceModule>(sdr::path_from_utf8(module_path));
            auto devices = module->devices();
            if (devices.empty())
                throw std::runtime_error("Device module has no connected devices");
            const auto key = device == "synthetic" ? devices.front().id : device;
            source = std::make_unique<sdr::DeviceSource>(module, key, config);
        }
        sdr::Demodulator demod;
        demod.configure(config);
        sdr::IqWriter writer;
        if (!output.empty())
            writer.open(sdr::path_from_utf8(output));
        const auto target = static_cast<std::uint64_t>(seconds * config.sample_rate);
        std::uint64_t iq = 0, audio = 0, gaps = 0;
        sdr::Block block;
        std::vector<float> samples;
        const auto start = std::chrono::steady_clock::now();
        auto last_data = start;
        while (iq < target) {
            if (!source->read(block))
                break;
            if (block.samples.empty()) {
                if (std::chrono::steady_clock::now() - last_data > std::chrono::seconds(5))
                    throw std::runtime_error("Source timeout");
                continue;
            }
            last_data = std::chrono::steady_clock::now();
            if (block.discontinuity) {
                ++gaps;
                demod.configure(config);
            }
            if (block.samples.size() > target - iq)
                block.samples.resize(static_cast<std::size_t>(target - iq));
            demod.process(block.samples, samples);
            if (!output.empty())
                writer.write(block.samples);
            iq += block.samples.size();
            audio += samples.size();
        }
        writer.close();
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (!output.empty()) {
            std::ofstream meta(sdr::path_from_utf8(output + ".json"));
            meta << "{\"version\":1,\"datatype\":\"cf32_le\",\"sample_rate\":" << config.sample_rate
                 << ",\"center_hz\":" << config.center_hz << ",\"samples\":" << iq
                 << ",\"complete\":" << (gaps ? "false" : "true")
                 << ",\"discontinuities\":[],\"origin\":\"benchmark capture\"}";
            if (!meta)
                throw std::runtime_error("Metadata write failed");
        }
        std::cout << "{\"input_rate\":" << config.sample_rate << ",\"iq_samples\":" << iq
                  << ",\"audio_samples\":" << audio << ",\"iq_gaps\":" << gaps
                  << ",\"elapsed_seconds\":" << elapsed
                  << ",\"processing_msps\":" << iq / elapsed / 1e6
                  << ",\"acceptance\":\"not_evaluated\"}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
