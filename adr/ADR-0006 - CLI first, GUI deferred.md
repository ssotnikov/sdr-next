# ADR-0006: CLI first, GUI deferred

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

The source → DSP → sink architecture has to be proven before a user
interface is built on it. The choice of GUI toolkit is constrained by
[ADR-0002](ADR-0002%20-%20Go%20without%20cgo.md): many Go GUI toolkits need
cgo.

## Decision

The first deliverable is the `sdrnext` CLI, which writes demodulated audio to
WAV. Live audio output and a GUI (spectrum and waterfall) come later. The GUI
toolkit will be chosen in a separate ADR.

## Consequences

- The pipeline can be exercised and tested end to end without any UI
  dependencies.
- The GUI toolkit choice stays open until it is decided.
