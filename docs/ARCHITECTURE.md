# Architecture

SDR Next is a software-defined radio receiver for Windows (AMD64, ARM64) and
Linux, written in Go. Architectural decisions are recorded as ADRs in
[adr/](../adr/README.md).

## Layers

```text
cmd/sdrnext                   CLI: flags, wiring, signal handling
   │
internal/receiver             demodulation chain, threading
   │            │
   │       internal/sink/wav  audio output (WAV today, audio devices later)
   │
internal/source               Source interface + driver registry
   ├── sim                    synthetic FM station (tests, demos)
   ├── iqfile                 raw I/Q playback (.cu8 .cs8 .cs16 .cf32)
   ├── hackrf  ─┐
   └── airspy  ─┴─ internal/usb    cgo-free USB: WinUSB | usbfs
                        └── usbtest    scripted fake device for driver tests
internal/dsp                  filters, decimators, demodulators, converters
```

Dependencies point downward only. `dsp` has no dependencies inside the
project, and drivers do not know about the receiver.

Planned additions:

```text
plugin/                       public plugin contract (Go API, versioned)
plugins/<id>/                 first-party built-in Go plugins
internal/pluginhost           plugin lifecycle, stream hooks; Lua, WASM (wazero)
                              and process (gRPC + shared memory) hosts
internal/ui                   Gio GUI (ADR-0007)
```

Already in place: `internal/ui/theme` loads the design-system themes
(built-in dark and light, user themes in JSON) with only the standard
library, and `internal/ui/theme/giotheme` adapts them to Gio with the
embedded IBM Plex fonts (ADR-0012).

## Plugins

Plugins extend the receiver through one contract (ADR-0008, spec in
[PLUGINS.md](PLUGINS.md)):

- **Built-in Go plugins** in `plugins/<id>/` are compiled into the binary.
  They can hook the I/Q and audio streams, add modes, sources and sinks, and
  draw Gio panels.
- **Lua scripts** are loaded at startup from `plugins/` next to the
  executable and from the user config directory. They automate the radio
  (tuning, events, timers, declarative UI) in a sandbox, without stream
  access.
- **WASM plugins** (Rust, C/C++, Zig, TinyGo) run sandboxed in wazero and
  may process streams (ADR-0010).
- **Process plugins** (Python, C#) are separate programs connected over
  gRPC, with shared-memory ring buffers for samples. They get stream taps
  only (ADR-0010).

Stream hooks attach at four points: raw I/Q, channel I/Q, demodulator
output and final audio. They run on the DSP goroutine under real-time rules.
SDR# and SDR++ plugins are not loaded directly; first-party equivalents of
the popular ones are planned instead (ADR-0009).

## Sources and drivers

A `source.Driver` enumerates and opens devices of one kind and registers
itself from `init`. `main` chooses which drivers exist by importing them.
Users address a source as `driver[:id]`, for example `hackrf`,
`airspy:<serial>` or `file:rec.cs8`.

A `source.Source` exposes sample rate, frequency, named gain stages and
`Stream`, which delivers `[]complex64` blocks scaled to about ±1. Each driver
converts its native format:

| Device | Wire format | Conversion |
|---|---|---|
| HackRF | interleaved int8 I/Q | `dsp.CS8ToComplex` |
| Airspy | unpacked 12-bit real, at 2× the output rate | scale, then `dsp.RealToIQ` (fs/4 shift, half-band, ÷2) |

## Receive chain

```text
IQ @ fs ─► channel filter ÷D1 ─► FM discriminator ─► de-emphasis ─► audio filter ÷D2 ─► sink
           (IF ≈ 250 kHz WFM,                        (WFM only)     (≈ 48 kHz)
                 ≈  50 kHz NFM)
```

D1 and D2 are whole numbers chosen to land close to the target rates, so the
audio rate is exact only when fs divides evenly (2.4 MS/s gives exactly
48 kHz). A rational resampler will remove that constraint.

Each decimation filter is a Blackman-windowed sinc whose -6 dB point sits at
the output Nyquist frequency and whose stopband starts where aliases would
fold into the passband. The tap count follows from that transition width.

## Threading

```text
source goroutine                  DSP goroutine (Receiver.Run)
Stream callback ─ copy ─► [queue, depth 8] ─► process ─► AudioWriter
        ▲                                        │
        └──────────── free buffer pool ◄─────────┘
```

- Nothing on the hot path allocates in steady state: DSP blocks own reusable
  buffers and the queue recycles its blocks.
- Cancelling the context stops everything. `Stream` returns nil on
  cancellation, and `Run` returns the first real error from either side.
- Source setters (`SetFrequency` and the rest) may be called while the source
  is streaming, so `Receiver.Tune` retunes live.

## Platform support

| | Windows AMD64 | Windows ARM64 | Linux AMD64 | Linux ARM64 |
|---|---|---|---|---|
| Build (`CGO_ENABLED=0`) | ✅ | ✅ | ✅ | ✅ |
| sim / file sources | ✅ | ✅ | ✅ | ✅ |
| USB backend | WinUSB (planned) | WinUSB (planned) | usbfs (planned) | usbfs (planned) |
| Audio output | WASAPI (planned) | WASAPI (planned) | ALSA/PipeWire (planned) | ALSA/PipeWire (planned) |

## Roadmap

1. USB backends: WinUSB (`usb_windows.go`) and usbfs (`usb_linux.go`).
2. Validate the HackRF and Airspy drivers on hardware, in particular Airspy
   tuning offset, spectrum orientation and sample scaling.
3. Live audio output (WASAPI, then ALSA or PipeWire) and a rational resampler.
4. AM/SSB demodulators; WFM stereo; selectable de-emphasis.
5. FFT, spectrum and waterfall; GUI in `internal/ui` on Gio (ADR-0007),
   starting with a waterfall performance prototype.
6. Plugin host: the `plugin` contract (API v0), stream hooks, the Lua
   runtime, then the Phase 1 first-party plugins (frequency manager,
   scanner, recorder, rigctl server, network sink); later phases are listed
   in [PLUGINS.md](PLUGINS.md).
7. Performance: SIMD-friendly FIR kernels and polyphase filtering; overrun
   detection.
