# sdr-next

SDR Next is a software-defined radio receiver for Windows (AMD64, ARM64) and
Linux, written in pure Go with no cgo. It will support Airspy and HackRF
natively, with no libusb, libairspy or libhackrf.

> **Status: early skeleton.** The receive pipeline runs end to end with the
> simulated and file sources. The Airspy and HackRF drivers implement their
> control protocols but cannot reach hardware until the WinUSB and usbfs
> backends land. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the
> design and roadmap, and the [ADRs](adr/README.md) for the reasoning behind
> it.

## Build

Requires Go 1.27 or newer.

Binaries are written to `dist/` with the version from `git describe`
embedded. Cross-compiling needs no C toolchain.

**Windows** (PowerShell 7):

```powershell
.\build.ps1                          # fmt + vet + test, then build windows/amd64 and windows/arm64
.\build.ps1 -Task build -Arch arm64  # one architecture
.\build.ps1 -Task cross              # all OS/architecture targets
.\build.ps1 -Task check              # fmt + vet + test only
```

Other tasks: `test`, `vet`, `fmt`, `vuln` (needs `govulncheck`), `clean`.
Set the version with `-Version 0.1.0`.

**Linux** (including WSL Ubuntu):

```bash
make          # fmt + vet + test, then build for the host architecture
make linux    # linux/amd64 and linux/arm64
make cross    # all OS/architecture targets
make race     # tests with the race detector (needs gcc: sudo apt install build-essential)
```

Other targets: `test`, `vet`, `fmt`, `vuln`, `check`, `clean`. Set the
version with `make VERSION=0.1.0`.

## Usage

```bash
sdrnext devices
sdrnext rx -out fm.wav -duration 5s                     # simulated station, 1 kHz tone
sdrnext rx -device file:capture.cs8 -rate 10M -freq 100.1M -out fm.wav
sdrnext rx -device hackrf -freq 100.1M -rate 10M -gain LNA=16,VGA=20 -out fm.wav
```

Run `sdrnext rx -h` for all flags. Supported modes are `wfm` and `nfm`.

## Hardware setup (once the USB backends land)

- **Windows:** the device must use the WinUSB driver. If it does not appear,
  bind it with [Zadig](https://zadig.akeo.ie/).
- **Linux:** install the udev rules shipped by the
  [airspy](https://github.com/airspy/airspyone_host) or
  [hackrf](https://github.com/greatscottgadgets/hackrf) projects so the device
  is accessible without root.

## Development

Development follows TDD and security by design; see [AGENTS.md](AGENTS.md)
and ADR-0011. Run `.\build.ps1 -Task check` or `make check` before every
commit.

See [CONTRIBUTING.md](CONTRIBUTING.md) for branch, commit and PR conventions.

## License

[MIT](LICENSE)
