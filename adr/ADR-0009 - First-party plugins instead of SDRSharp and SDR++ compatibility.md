# ADR-0009: First-party plugins instead of SDR# and SDR++ compatibility

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

Running existing SDR# plugins and SDR++ modules inside SDR Next would be a
strong competitive advantage. The analysis showed that it is not feasible:

- **SDR# plugins** are .NET assemblies implementing `ISharpPlugin`. They use
  `ISharpControl` and stream hooks, and reference the closed
  `SDRSharp.Common`, `SDRSharp.Radio` and `SDRSharp.PanView` assemblies.
  Their UI is WinForms. Hosting them would mean embedding the .NET runtime,
  reimplementing a closed, frequently changing API (the move to .NET 9 in
  2024 broke plugins), and embedding WinForms windows. That is Windows-only,
  fragile and legally risky.
- **SDR++ modules** export C symbols (`_INFO_`, `_INIT_`,
  `_CREATE_INSTANCE_`, …) but are C++ objects linked against `sdrpp_core`,
  whose objects they call directly. They draw with Dear ImGui and work only
  with the exact core build they were compiled for. Hosting them means
  embedding SDR++ itself, which is incompatible with pure Go and would make
  SDR Next GPL-3.0.

## Decision

- SDR Next does not pursue binary compatibility with SDR# plugins or SDR++
  modules.
- Instead, we build first-party plugins on our own plugin system
  ([ADR-0008](ADR-0008%20-%20Plugin%20system%20with%20Go%20plugins%20and%20Lua%20scripts.md)).
  They cover the most popular functionality of both ecosystems, chosen by
  popularity and user value.
- The plan and priorities live in [docs/PLUGINS.md](../docs/PLUGINS.md)
  under "First-party plugin roadmap" and are revised as the project
  progresses.
- Where it helps users migrate, plugins import data from SDR# and SDR++, for
  example frequency lists and bookmarks.
- First-party plugins are written from their functional behaviour and public
  documentation, not by copying source code. SDR++ is GPL-3.0 and most SDR#
  plugins are closed or carry their own licenses.

## Consequences

- Plugins behave the same on Windows AMD64/ARM64 and Linux and integrate
  natively with the Gio UI.
- We carry the development cost of every plugin; the ecosystem does not come
  for free.
- Digital voice decoders touch patented codecs (AMBE in DMR/P25/dPMR/NXDN,
  ACELP in TETRA). Each one needs a licensing review, and a separate ADR
  where warranted, before it is implemented.
- Interoperability through standard protocols (rigctl/Hamlib, network I/Q
  and audio, SpyServer) remains a separate question; this ADR does not
  decide it.
