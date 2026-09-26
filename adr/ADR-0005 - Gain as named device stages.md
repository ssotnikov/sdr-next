# ADR-0005: Gain as named device stages

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

Neither the Airspy nor the HackRF front end is calibrated in dB, and each
gain stage has different noise and linearity trade-offs.

## Decision

Sources expose their gain elements (`LNA`, `VGA`, `MIXER`, `AMP`) in the
device's own units rather than as a single dB value.

## Consequences

- Users and higher layers keep full control over each stage.
- A combined "gain" or "linearity/sensitivity" control can be built on top
  later without hiding the stages.
