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

```bash
go build ./cmd/sdrnext
```

Cross-compiling needs no C toolchain:

```bash
CGO_ENABLED=0 GOOS=windows GOARCH=arm64 go build -o dist/sdrnext-windows-arm64.exe ./cmd/sdrnext
CGO_ENABLED=0 GOOS=linux   GOARCH=amd64 go build -o dist/sdrnext-linux-amd64      ./cmd/sdrnext
```

In PowerShell, set the variables first, e.g. `$env:GOOS="linux"; $env:GOARCH="arm64"`.

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

```bash
go test ./...
go vet ./...
gofmt -l .
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for branch, commit and PR conventions.

## License

[MIT](LICENSE)
