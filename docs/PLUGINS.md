# Plugin specification (draft, API v0)

This is the working specification of the SDR Next plugin contract decided in
[ADR-0008](../adr/ADR-0008%20-%20Plugin%20system%20with%20Go%20plugins%20and%20Lua%20scripts.md)
and extended to more languages in
[ADR-0010](../adr/ADR-0010%20-%20WASM%20and%20external-process%20plugins.md).
Unlike ADRs, this document evolves. Until API v1 is released, anything here
may change.

## 1. Plugin kinds

All kinds implement the same contract (§2); only the transport differs.

| Kind | Languages | Where it lives | Loaded | Stream hooks | UI |
|---|---|---|---|---|---|
| Built-in | Go | `plugins/<id>/` in this repository (or a third-party module compiled in) | At build time, by import | processors and taps | Gio or declarative |
| Script | Lua 5.1 | runtime plugin directory (below) | At startup; can be reloaded | none | declarative |
| WASM | Rust, C/C++, Zig, TinyGo, Go (`wasip1`) | runtime plugin directory | At startup (wazero) | processors and taps | declarative |
| Process | Python, C#/.NET (any gRPC language) | runtime plugin directory | Started and supervised by the host | taps only | declarative or own window |

The runtime plugin directory is `plugins/<id>/` next to the executable, or
`<user config dir>/sdr-next/plugins/<id>/`.

A plugin ID is lowercase kebab-case and globally unique, e.g.
`frequency-manager`. If two plugins share an ID, the precedence is:
built-in, then the user config directory, then the directory next to the
executable. A warning is logged.

## 2. Go contract

Package `github.com/ssotnikov/sdr-next/plugin`.

```go
// APIVersion is the plugin API major version this build implements.
const APIVersion = 0

type Manifest struct {
    ID          string // kebab-case, unique
    Name        string // shown in the UI
    Version     string // semver of the plugin itself
    APIVersion  int    // must equal plugin.APIVersion
    Author      string
    License     string // SPDX identifier
    Description string
}

type Plugin interface {
    Manifest() Manifest
    // Init is called once, after the host is ready. The plugin registers
    // its capabilities through host.
    Init(host Host) error
    // Close releases resources. Hooks are already unregistered when it runs.
    Close() error
}

// Register makes a plugin factory available. Call it from init().
func Register(factory func() Plugin)
```

### 2.1 Host API

`Host` is what a plugin can see and do. Each area is a small interface, so
the host can grow without breaking plugins.

| Area | Capabilities |
|---|---|
| `Radio()` | center and tuned frequency, mode, bandwidth, squelch state; set/tune; start/stop |
| `VFOs()` | create extra channels (VFOs) with their own offset, bandwidth and rate, e.g. for decoders and multi-channel recording |
| `Streams()` | register stream hooks (§2.2) |
| `Modes()` | register demodulators as new modes |
| `Sources()` / `Sinks()` | register source drivers and audio/data sinks |
| `Spectrum()` | subscribe to FFT frames at a requested rate |
| `Events()` | subscribe to host events (§2.3) |
| `Config()` | per-plugin persistent key/value configuration |
| `UI()` | contribute a panel, menu items and spectrum overlays (§2.4) |
| `Log()` | structured logging tagged with the plugin ID |

### 2.2 Stream hooks

Hook points are modelled on the SDR# `ProcessorType` points and the SDR++
signal path:

| Point | Data | Rate |
|---|---|---|
| `RawIQ` | `[]complex64` from the source | device rate |
| `ChannelIQ` | `[]complex64` after the channel filter of a VFO | IF rate |
| `Demodulated` | `[]float32` straight out of the demodulator (FM: MPX) | IF rate |
| `Audio` | `[]float32` final audio before the sink | audio rate |

A hook is either a **processor**, which may modify the buffer in place, or
a **tap**, which gets a read-only copy. Processors run in registration order
(an explicit priority can be added later).

**Real-time rules:**
- Hooks run on the DSP goroutine.
- They must not block, do I/O or allocate in steady state.
- Heavy work belongs in the plugin's own goroutine, fed through the
  `plugin.Ring` helper, which drops and counts blocks when it is full.
- The host measures the time spent in each hook and warns about slow
  plugins.

### 2.3 Events

`FrequencyChanged`, `ModeChanged`, `BandwidthChanged`, `SquelchOpened`,
`SquelchClosed`, `Started`, `Stopped`, `SourceChanged`, `Tick` (1 Hz),
`Shutdown`. Event handlers run on the host's event goroutine, never on the
DSP goroutine.

### 2.4 UI

- **Go plugins** provide Gio widgets: a side panel (`layout.Widget`),
  optional spectrum and waterfall overlays (markers, band plans, bookmarks)
  and menu items. They must follow the host theme.
- **Declarative UI** (used by Lua, and optionally by Go) is a tree of host
  widgets: `label`, `button`, `toggle`, `slider`, `number`, `text`,
  `select`, `list`, `table`, `meter`, `row`, `column`, `group`. The host
  renders it and routes callbacks to the plugin.

### 2.5 Lifecycle and errors

1. Discover: registered Go factories and script directories.
2. Validate each manifest; reject any whose `APIVersion` is not
   `plugin.APIVersion`.
3. Call `Init` for plugins the user has enabled.
4. Run.
5. On shutdown, unregister hooks, then call `Close`.

The host recovers panics in every plugin callback. The failing plugin is
disabled, its hooks are removed and the user is told. Other plugins and
reception continue.

## 3. Lua contract

A script plugin is a directory holding `plugin.lua`:

```lua
plugin = {
  id = "band-scanner",
  name = "Band scanner",
  version = "0.1.0",
  api = 0,
  permissions = {},            -- e.g. { "file:read", "net:http" }
}

function init(host)
  local ui = host.ui.panel("Band scanner")
  ui:button("Start", function() start_scan(host) end)
  host.events.on("squelch_opened", function(ev) host.log.info("signal at " .. ev.frequency) end)
end
```

**Available to scripts:**
- `host.radio` — read and set frequency, mode, bandwidth and squelch.
- `host.events`, `host.timer` — events and timers.
- `host.config` — per-plugin configuration.
- `host.spectrum` — low-rate peak and level readings at 10 Hz at most.
- `host.ui` — declarative UI.
- `host.log`.

**Not available to scripts:** stream hooks and sample buffers.

**Sandbox:** the standard `os`, `io`, `debug` and `package.loadlib`
libraries are removed. `permissions` requests narrow host-provided
equivalents, which the user confirms the first time the script is enabled.

## 4. WASM and process plugins

Both kinds live in a runtime plugin directory with a `plugin.json`
manifest:

```json
{
  "id": "adsb-decoder",
  "name": "ADS-B decoder",
  "version": "0.1.0",
  "api": 0,
  "author": "…",
  "license": "MIT",
  "kind": "wasm",
  "entry": "adsb.wasm",
  "permissions": ["net:listen"]
}
```

For `"kind": "process"`, `entry` is a command line relative to the plugin
directory, e.g. `["python", "main.py"]` or `["MyPlugin.exe"]`.

### 4.1 WASM ABI

- Modules target WASI preview 1 and run in wazero.
- **Exports:** `sdrnext_api_version() -> i32`,
  `sdrnext_alloc(size) -> ptr`, `sdrnext_free(ptr)`,
  `sdrnext_init() -> i32`, `sdrnext_process(hook, point, ptr, len) -> i32`,
  `sdrnext_event(ptr, len)`, `sdrnext_close()`.
- **Imports:** the `sdrnext` host module provides one function per host
  operation (`radio_set_frequency`, `streams_register`, `ui_set_tree`, …).
  Structured values are encoded as JSON for control calls. Sample buffers
  are raw little-endian `complex64`/`float32` arrays in linear memory, so the
  hot path needs no serialization.
- **SDKs:** a Rust crate, a C header (also used from C++ and Zig) and a Go
  package for TinyGo and `GOOS=wasip1`. Each includes a template plugin.

### 4.2 Process protocol

- **Control:** gRPC over a Unix domain socket (Linux) or a named pipe
  (Windows). The `.proto` files mirror §2 service by service (`Radio`,
  `VFOs`, `Streams`, `Events`, `Config`, `UI`, `Log`). The host passes the
  endpoint in `SDRNEXT_PLUGIN_ENDPOINT`.
- **Samples:** each tap gets a single-producer ring buffer in shared memory,
  announced over gRPC. When a plugin falls behind, blocks are dropped and
  counted; the DSP goroutine never waits.
- **Output:** a process plugin can publish results as a new audio or data
  output (for example decoded audio), not by modifying the stream in place.
- **Supervision:** the process is restarted with back-off after a crash and
  disabled after repeated failures.
- **SDKs:** a Python package (NumPy views over the ring buffers) and a
  C#/.NET NuGet package (`Span<T>` views). The C# SDK includes an
  SDR#-style facade (`ISharpPlugin`/`ISharpControl`-like) that eases porting
  SDR# plugins at the source level.

### 4.3 Permissions and trust

- WASM plugins are sandboxed. `permissions` requests files, network or
  clock, and the user confirms on first enable.
- Process plugins are not sandboxed. The user must explicitly trust each
  one, and the host shows the declared permissions and the command it will
  run.

## 5. Versioning

- `plugin.APIVersion` is the major version. It changes only with breaking
  changes to §2, §3 or §4. The WASM ABI and `.proto` files are versioned
  with it.
- Additive changes (new host areas, events, widgets) keep the major version.
  A plugin can probe for optional host interfaces with type assertions (Go)
  or `host.has("…")` (Lua).
- v0 carries no compatibility promise. v1 freezes the contract once the
  first set of first-party plugins proves it.

## 6. First-party plugin roadmap

This implements
[ADR-0009](../adr/ADR-0009%20-%20First-party%20plugins%20instead%20of%20SDRSharp%20and%20SDR%2B%2B%20compatibility.md).
Priorities are based on the most popular SDR# plugins (rtl-sdr.com
catalogue) and the modules bundled with SDR++. "Lua" marks plugins that can
double as example scripts.

### Phase 1: everyday tools (proves the API)

| Plugin | SDR# counterpart | SDR++ counterpart | Notes |
|---|---|---|---|
| `frequency-manager` | Frequency Manager + Scanner Suite | frequency_manager | Bookmarks and groups on the waterfall; **import of SDR# and SDR++ bookmark files** |
| `scanner` | Fast Scanner, Frequency Scanner | scanner | Scans lists and ranges with squelch-based dwell; uses frequency-manager lists |
| `recorder` | Modified Audio/Baseband Recorder, IF Recorder | recorder | Audio, baseband and channel I/Q recording; squelch-triggered |
| `rigctl-server` | Gpredict Connector, Net Remote, CalicoCAT | rigctl_server | Hamlib protocol: Gpredict Doppler, WSJT-X, logging software |
| `network-sink` | (various TCP/UDP audio plugins) | network_sink, iq_exporter | Audio and I/Q over TCP/UDP to external decoders (DSD+, multimon-ng, WSJT-X) |

### Phase 2: signal and audio tools

| Plugin | SDR# counterpart | SDR++ counterpart | Notes |
|---|---|---|---|
| `scheduler` (Lua) | recorder schedulers, DDE Scheduler | scheduler | Timed tuning and recording |
| `tone-squelch` | CTCSS/DCS detectors | — | CTCSS and DCS decode and squelch |
| `audio-processor` | Digital Audio Processor | — | Noise reduction, notch, band-pass, AGC tweaks |
| `if-notch` | IF Notch | — | Manual and auto notch in ChannelIQ |
| `level-meter` | Level Meter, IF Average | — | S-meter logging, averaging (radio astronomy) |
| `audio-waterfall` | Audio Waterfall | — | Waterfall of demodulated audio |
| `file-player` | File Player | (file source) | Timeline for I/Q recordings, including SDR# and SDR++ WAV captures |
| `rigctl-client` | Unitrunker Serial | rigctl_client | Follows an external rig, trunking controller |
| `frequency-lock` (Lua) | Frequency Lock | — | Example script |

### Phase 3: decoders

| Plugin | SDR# counterpart | SDR++ counterpart | Notes |
|---|---|---|---|
| `rds` | RDS Logger, MPX Output | (radio module RDS) | RDS decode and logging; MPX output |
| `pager` | (external: PDW) | pager_decoder | POCSAG and FLEX |
| `weather-sat` | QPSK Demodulator (Meteor) | meteor_demodulator, weather_sat_decoder | Meteor-M LRPT and NOAA APT |
| `m17` | — | m17_decoder | Open standard; needs a pure-Go Codec2 |
| `dab` | — | dab_decoder | DAB/DAB+ |
| `vor` | — | vor_receiver | Aviation VOR bearing |
| `sstv` | — | kg_sstv_decoder | SSTV images |
| `digital-voice` | DSD+ GUI, Simple DMR/P25, TETRA | — | **Blocked on licensing review** (AMBE/ACELP patents); may be limited to protocol and metadata decoding |

### Phase 4: showcase and experiments

ATV (`atv_decoder`), Falcon 9 telemetry (`falcon9_decoder`), passive radar,
eye diagram (Magic-Eye), Discord presence (`discord_integration`, Lua).
