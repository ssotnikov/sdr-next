# ADR-0001: Record decisions as immutable ADRs

- **Status:** Accepted
- **Date:** 2026-09-26

## Context

Architectural and technical decisions were being collected in a single
`docs/DECISIONS.md` that would be edited over time. Editing a shared document
in place loses the history of why a decision changed, and the reasoning
behind each version is hard to recover from git history alone.

## Decision

Every architectural or technical decision is recorded as a separate file in
`adr/`.

- **File name:** `ADR-<NNNN> - <very short description>.md`. The number is
  zero-padded to four digits, sequential, and never reused. Example:
  `ADR-0007 - GUI toolkit Gio.md`.
- **Language:** English.
- **Content:** title, status, date, context, decision and consequences. An ADR
  that replaces earlier decisions also has a *Supersedes* section.
- **Immutability:** once an ADR is accepted and written, its file is never
  modified. Not even its status changes.
- **Changing a decision:** write a new ADR. It links to the file of the
  previous decision and explains why that decision is being changed. It may
  supersede a decision entirely or only in part; it says which.
- **Index:** `adr/README.md` lists all ADRs and shows which are superseded.
  It is not an ADR and is updated whenever an ADR is added.
- `docs/DECISIONS.md` is retired. Its entries D1–D5 are carried over
  unchanged as ADR-0002 to ADR-0006.

## Consequences

- The full reasoning trail stays readable in the repository without digging
  through git history.
- To find the current state of a decision, read the index or follow the
  chain of superseding ADRs. The older file never says that it has been
  superseded.
- Typos in an accepted ADR stay. Only a mistake that changes the meaning
  justifies a new ADR.
