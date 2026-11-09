#include <iostream>
#include <sdr/device.hpp>
#include <stdexcept>
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F test) {
    bool rejected = false;
    try {
        test();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Device operation must be rejected");
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc >= 13, "Expected SDK example and eleven negative fixtures");
        auto module = std::make_shared<sdr::DeviceModule>(argv[1], "org.sdr.example.device");
        const auto devices = module->devices();
        check(devices.size() == 2 && devices[0].gain && devices[0].ppm && devices[0].bias_t &&
                  devices[0].direct_sampling,
              "Capabilities exposed");
        check(!devices[1].gain && !devices[1].ppm, "Capabilities differ per instance");
        sdr::Config c;
        c.sample_rate = 2400001;
        c.center_hz = 100000001;
        c.gain = 16;
        c.ppm = 1;
        c.bias_t = true;
        c.direct_sampling = 2;
        {
            sdr::DeviceSource source(module, devices[0].id, c);
            check(c.sample_rate == 2400000 && c.center_hz == 100000000 && c.gain == 16 &&
                      c.ppm == 1 && c.bias_t && c.direct_sampling == 2,
                  "Requested and applied parameters");
            module.reset();
            sdr::Block block;
            check(source.read(block) && block.samples.size() == sdr::block_size,
                  "Module retained during IQ read");
            sdr::DeviceReadTrace trace;
            check(source.read(block, &trace) && block.sample_index == sdr::block_size &&
                      block.sequence == 1 && block.samples.size() == sdr::block_size &&
                      trace.driver.driver_us == -1 && trace.conversion_us >= 0,
                  "Diagnostics fall back to the legacy ABI and measure conversion");
        }
        module = std::make_shared<sdr::DeviceModule>(argv[1]);
        rejects([&] { sdr::DeviceSource source(module, "example:1", c); });
        rejects([&] { sdr::DeviceModule invalid(argv[1], "wrong.id"); });
        for (int i = 0; i < 11; ++i) {
            const auto path = argv[i + 2];
            if (i <= 4)
                rejects([&] { sdr::DeviceModule bad(path, "org.sdr.test.device"); });
            else {
                auto bad = std::make_shared<sdr::DeviceModule>(path, "org.sdr.test.device");
                if (i == 7 || i == 10)
                    rejects([&] { bad->devices(); });
                else if (i == 8)
                    rejects([&] { sdr::DeviceSource source(bad, "test:bad", c); });
                else {
                    sdr::DeviceSource source(bad, "test:bad", c);
                    sdr::Block block;
                    rejects([&] { source.read(block); });
                }
            }
        }
        std::cout << "Device provider ABI tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
