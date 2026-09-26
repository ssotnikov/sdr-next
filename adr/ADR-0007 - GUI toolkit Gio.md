# ADR-0007: GUI toolkit Gio

- **Status:** Accepted
- **Date:** 2026-09-26

## Supersedes

- [ADR-0002 - Go without cgo](ADR-0002%20-%20Go%20without%20cgo.md),
  **in part:** cgo is now allowed for Linux GUI builds only.
- [ADR-0006 - CLI first, GUI deferred](ADR-0006%20-%20CLI%20first,%20GUI%20deferred.md),
  **in part:** the GUI toolkit is now chosen. CLI first still stands.

**Why they change.** No mature Go GUI toolkit opens a window on Linux
without cgo, because the Wayland, X11 and EGL client libraries are C. A strict
no-cgo rule would therefore rule out a native Linux GUI entirely. ADR-0006
deferred the toolkit choice until it was evaluated; that evaluation is this
ADR.

## Context

The GUI is dominated by custom real-time graphics: spectrum, waterfall,
frequency scale and a draggable filter passband, redrawn at 30–60 frames per
second. Windows ARM64 is a first-class target, and Snapdragon devices have
no native desktop OpenGL driver: OpenGL only works there through Microsoft's
GLon12 compatibility pack.

Options evaluated (GitHub stars / importers on pkg.go.dev as of 2026-09-26):

| Option | cgo | Windows ARM64 | Notes |
|---|---|---|---|
| **Gio** v0.10 (2.3k stars on a GitHub mirror; 909 importers) | Windows: no (Direct3D 11). Linux: yes | Native D3D11 | Immediate mode, GPU vector renderer; pre-1.0; few ready-made widgets |
| Fyne v2.8 (28.7k; 2,717 importers) | Yes, everywhere (GLFW + OpenGL) | Needs the OpenGL compatibility pack | Most widgets and largest community; removing cgo on Windows (fyne-io/fyne#911) is still open |
| Wails v3 beta (36.4k; ~550 importers) | Windows: no. Linux: yes (GTK4/WebKitGTK) | Yes | Web UI; second language (TS/JS); per-frame Go→JS IPC |
| Browser UI over HTTP/WebSocket | No | Yes | Not a desktop app; audio latency in the browser |
| Dear ImGui (giu / cimgui-go) | Yes, plus C++ | Hard | Proven for SDR (SDR++), hardest to cross-build |
| Qt (miqt) / GTK (gotk4) | Yes | Hard | Heavy runtimes; therecipe/qt abandoned since 2024 |

## Decision

The GUI is built with **Gio** (`gioui.org`).

- The GUI lives in its own package (`internal/ui`). The core (`dsp`,
  `source`, `usb`, `receiver`) must not import Gio and stays buildable with
  `CGO_ENABLED=0`.
- Windows builds (AMD64 and ARM64), including the GUI, keep
  `CGO_ENABLED=0`.
- Linux GUI builds use cgo with Wayland/X11/EGL development packages and are
  built natively on Linux runners (AMD64 and ARM64) rather than
  cross-compiled.
- The Gio version is pinned in `go.mod` and upgraded deliberately.

## Consequences

- Windows AMD64 and ARM64 keep cgo-free cross-compilation, and rendering
  does not depend on OpenGL.
- Immediate mode fits real-time spectrum and waterfall rendering and custom
  SDR controls.
- Gio has fewer ready-made widgets than Fyne; more UI code is ours.
- Gio is pre-1.0, so API changes are possible on upgrades.
- The CI matrix needs native Linux runners for GUI builds.
- Before building the full GUI, a prototype must confirm performance: a
  2048-bin waterfall at 60 frames per second, measured on Windows ARM64 and
  Linux. If it falls short, that result goes into a new ADR.
