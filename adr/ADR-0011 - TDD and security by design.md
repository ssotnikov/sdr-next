# ADR-0011: Test-driven development and security by design

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

SDR Next handles untrusted input from many directions:

- USB devices and firmware.
- Recordings and configuration files.
- Network peers (rigctl, network sinks, gRPC).
- Third-party plugins in several languages
  ([ADR-0008](ADR-0008%20-%20Plugin%20system%20with%20Go%20plugins%20and%20Lua%20scripts.md),
  [ADR-0010](ADR-0010%20-%20WASM%20and%20external-process%20plugins.md)).

Its DSP and protocol code is also easy to get subtly wrong. Quality and
security have to be built in from the start rather than bolted on.

## Decision

1. **Development follows test-driven development:** red → green →
   refactor.
   - No production code is written without a failing test that requires
     it, and bug fixes start with a reproducing test.
   - Tests are deterministic and use fakes instead of real time, network
     or hardware.
   - Parsers and protocol code are also covered by fuzz tests.

2. **Security by design, secure architecture and secure coding apply to
   every change:**
   - All external input is untrusted and validated at the boundary.
   - Components run with least privilege (sandboxed plugins, explicit trust
     for process plugins, localhost-only network services by default).
   - Failures fail safe.
   - The attack surface is kept minimal.
   - Every new trust boundary is documented with a threat note.
   - Errors are always handled, secrets are never committed or logged, and
     dependencies are vetted with `govulncheck`.

The operational rules are in `AGENTS.md` under "Test-driven development"
and "Security by design".

## Consequences

- Features take longer to start but regress less. Tests double as the
  executable specification.
- Code must be designed for testability: injected clocks, sources and
  transports, as `usbtest.Fake` and the `sim` source already do.
- CI gains fuzzing and `govulncheck` jobs as the relevant code appears.
- Designs that add trust boundaries (network services, plugin hosts, file
  formats) include a threat analysis before implementation.
