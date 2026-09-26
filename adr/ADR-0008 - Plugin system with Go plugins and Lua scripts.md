# ADR-0008: Plugin system with built-in Go plugins and Lua scripts

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

A rich plugin ecosystem is a major reason people choose SDR# and SDR++.
SDR Next needs its own plugin specification. Binary compatibility with SDR#
(.NET, WinForms, closed API) and SDR++ (C++ ABI tied to one core build,
GPL-3.0) is not feasible; see [ADR-0009](ADR-0009%20-%20First-party%20plugins%20instead%20of%20SDRSharp%20and%20SDR%2B%2B%20compatibility.md).

Loading mechanisms considered:

| Mechanism | Verdict |
|---|---|
| Built-in Go packages compiled into the binary | **Chosen.** Native speed, direct access to Gio and the DSP API, no cgo |
| Lua scripts via a pure-Go interpreter | **Chosen**, for automation and light logic that can be added without rebuilding |
| Go standard library `plugin` package (.so) | Rejected: no Windows support, needs cgo, host and plugin must be built with identical toolchains and dependencies |
| WebAssembly via wazero | Not chosen for now (sandboxed, multi-language, but slower DSP and declarative UI only) |
| External processes over gRPC and shared memory | Not chosen for now (any language, crash isolation, but IPC cost and more moving parts) |

The rejected-for-now mechanisms can be added later by a new ADR without
changing the plugin contract, which is defined independently of how a plugin
is loaded.

## Decision

1. **One plugin contract.** It is defined in a public Go package
   `github.com/ssotnikov/sdr-next/plugin` (outside `internal/`, so plugins in
   other repositories can compile against it). It is versioned separately
   from the application, and a plugin declares the API major version it was
   written for. The draft specification is in
   [docs/PLUGINS.md](../docs/PLUGINS.md).

2. **Built-in Go plugins.**
   - Source code lives in `plugins/<id>/` in this repository; each package
     registers itself from `init`.
   - A plugin is included in a build by importing it; later, a builder tool
     can produce custom binaries with third-party Go plugins.
   - Go plugins may use every capability: I/Q and audio stream hooks,
     demodulators, sources and sinks, spectrum, events, and UI panels drawn
     directly with Gio.

3. **Lua scripts.**
   - Loaded at startup from a `plugins/` directory next to the executable and
     from `<user config dir>/sdr-next/plugins/`.
   - Interpreter: a pure-Go Lua 5.1 implementation (gopher-lua or
     equivalent), so [ADR-0002](ADR-0002%20-%20Go%20without%20cgo.md) holds.
   - Scripts get a subset of the contract: tuning and mode control, events,
     timers, per-plugin configuration, low-rate spectrum and level readings,
     and declarative UI rendered by the host.
   - Scripts get no sample-rate stream hooks: Lua is too slow for DSP.
   - Scripts run sandboxed. File, network and process access are off unless
     the script's manifest requests them and the user grants them.

4. **Isolation.** The host recovers panics and Lua errors raised in plugin
   callbacks, disables the failing plugin and reports it. Stream hooks run on
   the DSP goroutine and must not block.

## Consequences

- First-party plugins run at native speed and can offer rich Gio UIs.
- Adding or updating a Go plugin requires a new build of the application.
  Only Lua scripts can be installed by users at runtime.
- Plugin authors can use Go or Lua. Other languages need the WASM or
  external-process mechanisms, which require a new ADR.
- The public `plugin` package becomes a compatibility surface: breaking
  changes need a new API major version.
- The existing `source.Register` driver registry must become reachable
  through the plugin contract, so sources can be delivered as plugins.
