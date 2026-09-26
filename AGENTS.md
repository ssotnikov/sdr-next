# AGENTS.md

## Project overview

SDR Next is a multiplatform software-defined radio receiver written in Go for
Windows (AMD64, ARM64) and Linux (AMD64, ARM64). It is receive-only, drives
Airspy and HackRF natively over USB without cgo, has a Gio GUI and a plugin
system (Go, Lua, WASM, external processes).

## Key documents

Read the relevant ones before starting a task.

| Document | Purpose | In git |
|---|---|---|
| `docs/SPECIFICATION.ru.md` | Detailed development specification, in Russian: requirements FR-/NFR-/SEC-, open questions | **No** (local only) |
| `docs/DEVELOPMENT-PLAN.ru.md` | Staged, decomposed development plan, in Russian: tasks, tests, DoD, gates | **No** (local only) |
| `docs/ARCHITECTURE.md` | Architecture overview: layers, threading, platforms, roadmap | Yes |
| `docs/PLUGINS.md` | Plugin contract (API v0) and first-party plugin roadmap | Yes |
| `adr/README.md` | Index of Architecture Decision Records | Yes |
| `CONTRIBUTING.md` | Branch, commit, PR, release conventions | Yes |

- The two Russian documents are working documents of the product owner.
  Keep them in Russian and never commit them; they are listed in
  `.gitignore`.
- Tasks refer to plan IDs (e.g. "task 1.4") and requirement IDs (e.g.
  `FR-DSP-2`).
- ADRs take precedence over the specification. If a task conflicts with an
  accepted ADR, say so before proceeding.

## Repository layout

```text
cmd/sdrnext/        CLI entry point (GUI and server later)
internal/dsp/       filters, decimators, demodulators, converters
internal/source/    Source interface, registry, drivers (sim, iqfile, hackrf, airspy)
internal/usb/       cgo-free USB layer (WinUSB, usbfs), usbtest fake
internal/receiver/  demodulation chain and threading
internal/sink/wav/  WAV output
internal/ui/theme/  design-system themes (JSON, stdlib only); giotheme/ adapts them to Gio
plugin/             public plugin contract (planned)
plugins/<id>/       first-party built-in Go plugins
assets/icon/        master application icon (SVG, PNG)
packaging/          platform packaging assets (Windows .ico/MSIX, Linux icons and .desktop)
adr/                Architecture Decision Records
docs/               architecture, plugin spec, local Russian spec and plan
docs/private/       local-only owner material, e.g. the brand book (git-ignored)
```

## Build and test commands

Windows (PowerShell 7):

```powershell
.\build.ps1                  # fmt + vet + test, then build windows/amd64 and windows/arm64
.\build.ps1 -Task check      # fmt + vet + test
.\build.ps1 -Task cross      # build all four targets
.\build.ps1 -Task vuln       # govulncheck (must be installed)
```

Linux / WSL Ubuntu:

```bash
make            # fmt + vet + test, then build for the host
make check      # fmt + vet + test
make race       # tests with the race detector (needs gcc)
make cross      # build all four targets
make vuln       # govulncheck (must be installed)
```

- Binaries go to `dist/`.
- Builds use `CGO_ENABLED=0` (ADR-0002); only the Linux GUI and `make race`
  need cgo (ADR-0007).
- Run `check` before every commit and report the real results. Run `race`
  for concurrent code.

## Code style

- Standard Go style: `gofmt`, `go vet` clean. Match the surrounding code's
  naming, comment density and idioms.
- Core packages (`dsp`, `source`, `usb`, `receiver`) must stay cgo-free and
  must not import the GUI or plugin hosts.
- The DSP hot path does not allocate in steady state. Reuse buffers, and do
  not block the DSP goroutine.
- Comments explain *why*; package docs describe the role of the package.
- Keep dependencies minimal, maintained and license-compatible (MIT, BSD,
  Apache-2.0, ISC; OFL-1.1 for fonts); no GPL code.
- UI code takes colours, type styles, spacing and sizes from the theme
  (`internal/ui/theme`, ADR-0012) and never hard-codes them. Brand changes
  come from the brand book in `docs/private/brand-book/`, which is never
  committed.

## Testing instructions: TDD (mandatory, ADR-0011)

1. **Red:** write a failing test that states the required behaviour, and run
   it to see it fail for the expected reason.
2. **Green:** write the minimum code that makes it pass.
3. **Refactor:** clean up with all tests green.

- **No production code without a test that required it.** Bug fixes start
  with a test that reproduces the bug.
- **Tests are deterministic.** No real time, network or hardware in unit
  tests: use fakes (`usbtest.Fake`, the `sim` source), seeded randomness
  and injected clocks. Hardware tests go behind the `hil` build tag.
- **Coverage:**
  - DSP code gets numeric tests (gain, frequency, block-size independence)
    and golden vectors in `testdata/`.
  - Parsers and protocol code get table tests and Go fuzz tests
    (`testing.F`).
  - Concurrent code passes `go test -race`.
- **Target:** at least 80% line coverage for core packages.

## Security considerations: security by design (mandatory, ADR-0011)

Security is part of the architecture and of every change, not a later pass.

**Secure architecture**
- Treat everything outside the process as untrusted: USB devices and
  firmware responses, I/Q and recording files, network peers (rigctl,
  network sinks, gRPC, server mode), plugin manifests and plugin code, and
  configuration and import files.
- Least privilege:
  - Lua and WASM plugins are sandboxed, with capabilities granted explicitly
    by the user.
  - Process plugins need explicit trust.
  - Network services are off by default, listen on localhost only, and need
    an explicit opt-in to bind elsewhere.
- Fail safe: on an error, stop or disable the failing component, keep the
  rest running, and never fall back to a less secure mode silently.
- Keep the attack surface minimal: no unnecessary listeners, dependencies or
  permissions. Every new trust boundary or exposed interface needs a threat
  note in its ADR or design doc.

**Secure coding**
- Validate all external input at the boundary: lengths, ranges, counts,
  enums and paths.
  - Bound every allocation derived from external data.
  - Reject path traversal.
- No `unsafe`, `reflect` tricks or `os/exec` with shell interpolation
  outside dedicated, reviewed packages. Use `exec.Command` with explicit
  arguments.
- Handle every error; never discard errors from I/O, parsing or devices.
- Secrets never go in code, logs, test data or commits.
- Use `crypto/rand` for anything security-relevant and `math/rand` only for
  DSP and simulation.
- Run `govulncheck ./...` when adding or upgrading dependencies, and pin
  versions in `go.mod`.
- Security-relevant code (parsers, protocol handlers, plugin host,
  permission checks) has tests for malformed and hostile input, including
  fuzz tests.

## Architecture Decision Records (mandatory)

Every architectural or technical decision is recorded as an ADR in `adr/`.
Full rules are in `adr/ADR-0001 - Record decisions as immutable ADRs.md`.

- **One decision per file:** `adr/ADR-<NNNN> - <very short description>.md`,
  with the number zero-padded to four digits and sequential (take the highest
  existing number plus one; never reuse a number). Write ADRs in English.
- **Sections:** title, status, date, context, decision, consequences, plus
  *Supersedes* when the ADR replaces an earlier one.
- **Never modify an accepted ADR file**, not even its status or a typo.
- **To change a decision**, create a new ADR that links to the previous ADR
  file, says whether it supersedes it fully or in part, and explains why the
  decision changed.
- **Update `adr/README.md`** (the index, which is not itself an ADR) whenever
  you add an ADR, marking superseded decisions there.
- Plan tasks marked **[ADR]** start by writing the ADR; implementation
  follows only after the decision is accepted.
- `docs/DECISIONS.md` is retired; do not recreate it.

## Plugins

- The contract and roadmap are in `docs/PLUGINS.md` (ADR-0008, ADR-0009,
  ADR-0010).
- First-party Go plugins live in `plugins/<id>/` and register themselves
  from `init`.
- Runtime plugins (Lua, WASM, process) are loaded from `plugins/` next to the
  executable and from the user config directory. They are never committed
  under `plugins/`.
- Do not copy code from SDR++ (GPL-3.0) or SDR# plugins; implement from
  behaviour and public documentation (ADR-0009).

## Git and pull requests

Follow `CONTRIBUTING.md` for branch naming, Conventional Commits, pull
requests, reviews, and release workflow. In short: branches are named
`<type>/<short-description>`, and a PR description covers Summary,
Motivation, Changes, Testing and Related issues.

Keep each change focused on one logical purpose.

Do not create commits, push branches, open or merge pull requests, tag
releases, or otherwise modify remote repository state unless the current
task explicitly authorizes that action.

Before finishing, inspect:

```text
git status
git diff
git diff --staged
```

Do not include unrelated formatting changes, temporary output, local
configuration, caches, build artifacts, or secrets. In this repository the
local-only paths are `docs/*.ru.md`, `docs/private/`, `.claude/` and
`dist/`.

When creating commits:

- use Conventional Commits;
- preserve the human developer as the primary author;
- add AI attribution using `Co-authored-by` trailers at the end of the
  commit message;
- for Claude Code contributions, add:
  `Co-authored-by: Claude <noreply@anthropic.com>`
- for ChatGPT/Codex contributions, add:
  `Co-authored-by: Codex <codex@openai.com>`
- when both Claude Code and ChatGPT/Codex contributed to the same commit,
  add both trailers;
- separate the trailers from the commit message body with a blank line.
