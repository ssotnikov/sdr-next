// Package giotheme adapts a theme.Theme to Gio: the IBM Plex font collection,
// a material.Theme with the SDR Next palette, and text/metric helpers.
//
// It is kept separate from package theme so the theme parser and its tests do
// not depend on Gio (ADR-0007: the core does not import the GUI).
package giotheme

import (
	"embed"
	"fmt"
	"image/color"

	"gioui.org/font"
	"gioui.org/font/opentype"
	"gioui.org/text"
	"gioui.org/unit"
	"gioui.org/widget/material"

	"github.com/ssotnikov/sdr-next/internal/ui/theme"
)

// Typefaces registered by Collection.
const (
	Sans font.Typeface = "IBM Plex Sans"
	Mono font.Typeface = "IBM Plex Mono"
)

//go:embed fonts/*.ttf
var fontFS embed.FS

var fontFiles = []struct {
	file   string
	face   font.Typeface
	weight font.Weight
}{
	{"IBMPlexSans-Regular.ttf", Sans, font.Normal},
	{"IBMPlexSans-Medium.ttf", Sans, font.Medium},
	{"IBMPlexSans-SemiBold.ttf", Sans, font.SemiBold},
	{"IBMPlexMono-Regular.ttf", Mono, font.Normal},
	{"IBMPlexMono-Medium.ttf", Mono, font.Medium},
}

// Collection parses the embedded IBM Plex faces (Latin + Cyrillic).
// Typeface and weight are set explicitly: the files' own family names
// ("IBM Plex Sans Medm") would otherwise split one family into several.
func Collection() ([]font.FontFace, error) {
	var out []font.FontFace
	for _, f := range fontFiles {
		b, err := fontFS.ReadFile("fonts/" + f.file)
		if err != nil {
			return nil, err
		}
		faces, err := opentype.ParseCollection(b)
		if err != nil {
			return nil, fmt.Errorf("giotheme: %s: %w", f.file, err)
		}
		for _, ff := range faces {
			ff.Font.Typeface = f.face
			ff.Font.Weight = f.weight
			out = append(out, ff)
		}
	}
	return out, nil
}

// NewShaper returns a text shaper with the Plex collection only (no system
// fonts), so the UI renders identically on Windows and Linux (NFR-8).
func NewShaper() (*text.Shaper, error) {
	coll, err := Collection()
	if err != nil {
		return nil, err
	}
	return text.NewShaper(text.NoSystemFonts(), text.WithCollection(coll)), nil
}

// Material builds a material.Theme for widgets that use Gio's material set.
// Custom SDR Next widgets should read t (colours, styles, metrics) directly.
func Material(t *theme.Theme, shaper *text.Shaper) *material.Theme {
	th := material.NewTheme()
	th.Shaper = shaper
	th.Face = Sans
	th.TextSize = unit.Sp(13) // style "body"
	if s, ok := t.Style("body"); ok {
		th.TextSize = unit.Sp(s.Size)
	}
	th.Palette = material.Palette{
		Bg:         t.Colors.Bg1,
		Fg:         t.Colors.Ink,
		ContrastBg: t.Colors.Dial,
		ContrastFg: t.Colors.OnDial,
	}
	th.FingerSize = unit.Dp(t.Metrics.Size["control-sm"])
	return th
}

// Font returns the Gio font for a named text style ("body", "freq-xl", …).
func Font(t *theme.Theme, style string) (font.Font, unit.Sp, bool) {
	s, ok := t.Style(style)
	if !ok {
		return font.Font{Typeface: Sans}, unit.Sp(13), false
	}
	f := font.Font{Typeface: Sans, Weight: weight(s.Weight)}
	if s.Family == "mono" {
		f.Typeface = Mono
	}
	return f, unit.Sp(s.Size), true
}

// Label is material.Label configured with a named text style and colour.
func Label(th *material.Theme, t *theme.Theme, style, txt string, c color.NRGBA) material.LabelStyle {
	f, size, _ := Font(t, style)
	l := material.Label(th, size, txt)
	l.Font = f
	l.Color = c
	if s, ok := t.Style(style); ok && s.Size > 0 {
		l.LineHeight = unit.Sp(s.LineHeight)
	}
	return l
}

// Dp returns a spacing, radius or size token in dp (0 if unknown).
func Dp(t *theme.Theme, token string) unit.Dp {
	for _, m := range []map[string]float32{t.Metrics.Spacing, t.Metrics.Radius, t.Metrics.Size} {
		if v, ok := m[token]; ok {
			return unit.Dp(v)
		}
	}
	return 0
}

func weight(w int) font.Weight {
	// Gio weights are relative to Normal (400): Medium = +100, SemiBold = +200.
	return font.Weight(w - 400)
}
