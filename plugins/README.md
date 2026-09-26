# First-party plugins

Each subdirectory is a built-in Go plugin, `plugins/<id>/`, that registers
itself with `plugin.Register` from `init` and is compiled into `sdrnext`.

- Contract and roadmap: [docs/PLUGINS.md](../docs/PLUGINS.md)
- Decisions: [ADR-0008](../adr/ADR-0008%20-%20Plugin%20system%20with%20Go%20plugins%20and%20Lua%20scripts.md),
  [ADR-0009](../adr/ADR-0009%20-%20First-party%20plugins%20instead%20of%20SDRSharp%20and%20SDR%2B%2B%20compatibility.md)

Runtime plugins (Lua scripts, WASM modules and process plugins in Python,
C# and other languages) are not stored here. They are loaded from the
`plugins/` directory next to the executable and from the user config
directory.

No plugins are implemented yet; the first ones are listed under Phase 1 of
the roadmap.
