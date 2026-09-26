# ADR-0002: Go, built without cgo

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

SDR Next targets Windows AMD64, Windows ARM64, Linux AMD64 and Linux ARM64.
Go was chosen as the implementation language. cgo would require a C
toolchain for every target, and MinGW and libusb builds for Windows ARM64 are
particularly awkward.

## Decision

SDR Next is written in Go and every build uses `CGO_ENABLED=0`.

## Consequences

- Pure Go cross-compiles to every target from any host with `GOOS`/`GOARCH`,
  with no C toolchain, and produces a single static binary with nothing to
  install beside it.
- C libraries (libusb, libairspy, libhackrf, FFTW, PortAudio) cannot be
  linked. USB, audio output and any heavy DSP must be implemented in Go or
  reached through OS APIs via syscalls (`golang.org/x/sys`).
