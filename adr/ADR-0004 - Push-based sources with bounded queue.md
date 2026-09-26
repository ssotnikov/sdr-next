# ADR-0004: Push-based sources with a bounded queue

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

USB streaming is naturally callback-driven: queued transfers complete in
order. DSP time per block varies, and a stalled callback delays transfer
resubmission, which makes the device overrun.

## Decision

`source.Source.Stream` calls back with each block of samples. The receiver
copies each block into a pooled buffer and queues it (depth 8) for a separate
DSP goroutine.

## Consequences

- The queue absorbs DSP jitter without blocking transfer resubmission, and
  pooled buffers keep the steady state allocation-free.
- When DSP falls behind for longer than the queue covers, the callback blocks
  and the device overruns. That is acceptable for now; overrun detection and
  reporting are future work.
