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
6. Performance: SIMD-friendly FIR kernels and polyphase filtering; overrun
   detection.
