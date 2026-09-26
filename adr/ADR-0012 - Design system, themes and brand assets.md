# ADR-0012: Design system, themes and brand assets

- **Status:** Accepted
- **Date:** 2026-09-27

## Context

The GUI ([ADR-0007](ADR-0007%20-%20GUI%20toolkit%20Gio.md)) needs one visual
language across Windows and Linux: colours, type, spacing, VFO colours and
the waterfall palette. The specification also asks for dark and light
themes and user themes in JSON (FR-UI-5), user waterfall palettes
(FR-VIS-6) and HiDPI scaling (FR-UI-6).

The product owner has supplied a brand book: the SDR Next design system as
tokens, a ready Gio theme package generated from them, IBM Plex fonts, the
application icon for Windows and Linux, and the site logo. The brand book
is a private working asset and is not kept in git.

A theme file is also a new trust boundary: users download themes from
others, so the loader parses untrusted input
([ADR-0011](ADR-0011%20-%20TDD%20and%20security%20by%20design.md)).

## Decision

1. **The design system is the single source of UI styling.** Its tokens are
   shipped as theme documents in JSON, format 1:
   - two built-in themes, `sdr-next-dark` (default) and `sdr-next-light`,
     embedded in the binary;
   - user themes in the same format, with a published JSON Schema; values a
     user theme omits fall back to the built-in theme named by its `base`.
   UI code reads colours, type styles, spacing, radii, control sizes and
   opacities from the theme and does not hard-code them.

2. **Packages.**
   - `internal/ui/theme` loads and validates themes. It depends only on the
     standard library, so it is testable without a GPU and fuzzable.
   - `internal/ui/theme/giotheme` adapts a theme to Gio: the font
     collection, `material.Theme` and text-style helpers.
   - The core (`dsp`, `source`, `usb`, `receiver`) imports neither.

3. **Fonts.** IBM Plex Sans and IBM Plex Mono (Latin and Cyrillic) are
   embedded, and the text shaper uses them only, without system fonts, so
   the UI renders the same on every platform. Frequencies and readouts use
   Plex Mono. The fonts are licensed under **SIL OFL 1.1**, which is
   accepted for fonts and other non-code assets; the licence text ships
   with the fonts and with every release.

4. **Brand rules enforced by tests.**
   - Text contrast is at least 4.5:1 and non-text UI contrast at least
     3:1 (WCAG 2) for every built-in theme, including all VFO colours.
   - The focus ring uses the trace colour.
   - The default waterfall palette is "Next"; further palettes (FR-VIS-6)
     are additional theme palettes, not a separate mechanism.

5. **Theme files are untrusted input.**
   - Input is capped at 256 KiB, decoded strictly (unknown fields and
     trailing data are rejected), and every value is range-checked.
   - Identifiers match `^[a-z0-9][a-z0-9-]{0,63}$`; font file names are
     plain names, never paths.
   - Themes cannot load fonts or other files: the adapter uses only the
     embedded fonts.
   - The loader is fuzzed, and the JSON Schema is kept in step with the
     loader by tests.

6. **Brand assets in the repository.**
   - The master application icon (SVG and PNG) lives in `assets/icon/`.
   - Platform packaging assets live in `packaging/` (Windows `.ico` and
     MSIX logos; Linux hicolor icons and the `.desktop` entry).
   - The README logo lives in `docs/assets/`.
   - Brand changes come from an updated brand book and are brought in the
     same way, with tests updated first.

7. **Gio is pinned** to v0.10.2 in `go.mod`, as ADR-0007 requires.

## Consequences

- One source of truth for the look; custom widgets, plugin panels and
  declarative plugin UI (ADR-0008) share it.
- User themes are safe to exchange, and theme authors get editor
  validation from the schema.
- The binary grows by about 1 MB for the embedded fonts.
- Scripts other than Latin and Cyrillic are not covered by the embedded
  fonts. Adding localisations in other scripts needs another font and a
  new ADR.
- The OFL notice must be included in release artefacts and in the
  third-party licence list; the specification's licence list (NFR-11)
  gains OFL-1.1 for fonts.
- Brand-book changes can break the contrast tests; that is intended.
