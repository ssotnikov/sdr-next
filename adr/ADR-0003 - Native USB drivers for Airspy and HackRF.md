# ADR-0003: Own USB layer and native drivers instead of libairspy/libhackrf

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

Airspy and HackRF are the first devices to support. Their vendor libraries
(libairspy, libhackrf) and libusb are C libraries, which
[ADR-0002](ADR-0002%20-%20Go%20without%20cgo.md) rules out. Both devices use a
small, well-documented vendor protocol: a dozen or so control requests plus
one bulk IN endpoint.

## Decision

Airspy and HackRF are driven directly. `internal/usb` provides enumeration,
vendor control requests and bulk streaming, with WinUSB on Windows and usbfs
on Linux. The drivers in `internal/source/{airspy,hackrf}` implement the
vendor protocol themselves.

## Consequences

- Reimplementing the protocols is cheaper than maintaining a cgo build matrix.
- We own the correctness of both protocols and must track firmware changes.
- Airspy's real-to-I/Q conversion, done by libairspy on the host, is now
  ours (`dsp.RealToIQ`).
- On Windows the devices must be bound to WinUSB, through WCID firmware
  descriptors or Zadig.
- Driver control logic is unit-tested against `usbtest.Fake`; streaming still
  needs validation on real hardware.
