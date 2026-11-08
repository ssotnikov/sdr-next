# SDR Next

A desktop SDR receiver built with C++20 and Qt Quick. The current v0.1 implementation is a preview build: full hardware validation and endurance acceptance testing are not yet complete.

Implemented features include separate dynamically loaded HackRF, RTL-SDR, and Airspy modules using a common Device Provider API; file and synthetic IQ sources; AM/NFM/WFM demodulation; spectrum and waterfall displays; audio output; WAV/Raw IQ recording; settings; and native Source/Demodulator plugins. The application also works without hardware modules. Validated hardware and limitations are documented in the verification report; a single successful launch does not establish hardware qualification.

- [User guide](docs/USER_GUIDE.md)
- [Building and testing](docs/DEVELOPER_GUIDE.md)
- [Plugin SDK](docs/sdr/PLUGIN_SDK.md)
- [Device modules](docs/sdr/DEVICE_PLUGIN_API.md)
- [DSP contracts](docs/sdr/DSP_API.md)
- [Architecture](docs/sdr/README.md)
- [OpenSpec tasks](openspec/changes/plan-mvp-v0-1/tasks.md)
- [Verification results and remaining acceptance testing](docs/sdr/MVP_V0_1_VERIFICATION.md)

The project's own code is licensed under MIT. Qt, SoapySDR, and separately installed drivers have their own distribution terms. The application does not transmit telemetry automatically. TX, remote sources, and untrusted plugins are outside the scope of v0.1.
