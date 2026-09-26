# ADR-0010: WASM and external-process plugins

- **Status:** Accepted
- **Date:** 2026-09-26

## Supersedes

[ADR-0008 - Plugin system with Go plugins and Lua scripts](ADR-0008%20-%20Plugin%20system%20with%20Go%20plugins%20and%20Lua%20scripts.md),
**in part:**

- WebAssembly and external-process plugins, marked "not chosen for now",
  are now adopted.
- The consequence "only Lua scripts can be installed by users at runtime;
  other languages need a new ADR" no longer holds.

Everything else in ADR-0008 stands, in particular the single plugin
contract, built-in Go plugins, Lua scripts and the isolation rules.

**Why it changes.** Restricting plugins to Go and Lua cuts off the
languages most plugin authors actually use:

- Rust and C/C++ for performance-critical DSP.
- Python for scientific and ML work.
- C#, the language of the SDR# plugin ecosystem, whose authors are the
  most likely to port their work.

ADR-0008 was written so that these mechanisms could be added without
changing the contract, and this ADR does exactly that.

## Decision

Two more ways to load a plugin are added. Both implement the **same
contract** (package `plugin`, API version, manifest fields, host areas,
hook points, events, declarative UI, lifecycle). Only the transport differs.

Both kinds are installed at runtime in `plugins/<id>/`, next to the
executable or in the user config directory, and are described by a
`plugin.json` manifest. It carries the standard manifest fields plus `kind`
(`wasm` or `process`), the entry point and the requested permissions.

### WebAssembly plugins: Rust, C/C++, Zig, TinyGo

- **Runtime:** wazero, a pure-Go WebAssembly runtime with no cgo, so
  [ADR-0002](ADR-0002%20-%20Go%20without%20cgo.md) holds for the Windows
  builds.
- **Modules:** WASI preview 1 (`wasip1`) modules. One `.wasm` file runs on
  every OS and architecture.
- **Supported toolchains:** Rust (`wasm32-wasip1`), C/C++ (wasi-sdk/clang),
  Zig, TinyGo, and standard Go (`GOOS=wasip1` with `go:wasmexport`).
- **Interface:** a small C-style ABI.
  - Host functions are imported from the module `sdrnext`.
  - The plugin exports `sdrnext_api_version`, `sdrnext_init`,
    `sdrnext_alloc`, `sdrnext_free`, `sdrnext_process` and
    `sdrnext_close`.
  - Sample buffers are passed through the module's linear memory.
- **Capabilities:** all host areas. Stream hooks may be processors or taps
  at every point. The host warns if a WASM hook at `RawIQ` cannot keep up.
- **UI:** declarative only.
- **Sandbox:** WASM plugins are sandboxed by construction. Files, network
  and clock are available only through permissions granted by the user.

### External-process plugins: Python, C#

- **Process model:** the plugin is a separate program started and supervised
  by the host from the manifest's command line. It is restarted with
  back-off if it crashes.
- **Control channel:** gRPC over a local transport (a Unix domain socket on
  Linux, a named pipe on Windows), defined in versioned `.proto` files that
  mirror the Go contract.
- **Sample channel:** lock-free ring buffers in shared memory (a file mapping
  on Windows, a memfd mapping on Linux), implemented in pure Go on the host
  side.
- **Supported languages:** first-party SDKs for **Python** and **C#/.NET**.
  Any language with gRPC can implement the protocol.
  - The C# SDK includes a thin compatibility facade modelled on SDR#'s
    `ISharpPlugin`/`ISharpControl`, to make porting SDR# plugins at the
    source level easier.
  - This is not binary compatibility;
    [ADR-0009](ADR-0009%20-%20First-party%20plugins%20instead%20of%20SDRSharp%20and%20SDR%2B%2B%20compatibility.md)
    stands.
- **Capabilities:** all host areas, except that stream hooks at every point
  are **taps only**. Blocking the DSP goroutine on inter-process
  communication would break the real-time rules. A process plugin can
  inject results back as a new audio or data output, not by modifying the
  stream in place.
- **UI:** declarative UI rendered by the host, or the plugin's own window.
- **Trust:** process plugins are not sandboxed. The manifest must declare
  what the plugin does, and the user must explicitly trust the plugin before
  it is first started.

## Consequences

- Plugin authors can use Go, Lua, Rust, C/C++, Zig, TinyGo, Python and C#
  against one contract. Each additional host kind maps onto the same host
  areas.
- New dependencies: wazero, gRPC and protobuf, all pure Go. The core
  packages (`dsp`, `source`, `usb`, `receiver`) still must not depend on
  them; they are used only by `internal/pluginhost`.
- We maintain several SDKs: a Rust crate, a C header (also used from C++ and
  Zig), a TinyGo/Go wasip1 package, a Python package and a C# NuGet package.
  Each needs examples and CI.
- The `.proto` definitions and the WASM ABI become compatibility surfaces,
  versioned together with `plugin.APIVersion`.
- The host process takes on supervision, shared-memory lifetime and
  permission prompts. That code needs thorough testing, including crash and
  restart scenarios.
