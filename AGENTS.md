# SDR Next

Multiplatform SDR receiver in Go for Windows (AMD64, ARM64) and Linux. The
design is in `docs/ARCHITECTURE.md`; branch, commit and PR conventions are in
`CONTRIBUTING.md`.

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
- Do not use `docs/DECISIONS.md`; it has been retired.
- When a task involves an architectural or technical choice, check the
  existing ADRs first and follow them. If the task conflicts with an ADR, say
  so before proceeding.
