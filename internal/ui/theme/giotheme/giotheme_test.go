package giotheme

import (
	"image/color"
	"testing"

	"gioui.org/font"
	"gioui.org/unit"

	"github.com/ssotnikov/sdr-next/internal/ui/theme"
)

func TestCollectionHasPlexFamilies(t *testing.T) {
	coll, err := Collection()
	if err != nil {
		t.Fatal(err)
	}
	want := map[font.Typeface][]font.Weight{
		Sans: {font.Normal, font.Medium, font.SemiBold},
		Mono: {font.Normal, font.Medium},
	}
	got := map[font.Typeface]map[font.Weight]bool{}
	for _, f := range coll {
		if got[f.Font.Typeface] == nil {
			got[f.Font.Typeface] = map[font.Weight]bool{}
		}
		got[f.Font.Typeface][f.Font.Weight] = true
	}
	if len(got) != len(want) {
		t.Errorf("typefaces %v, want exactly Sans and Mono", got)
	}
	for face, weights := range want {
		for _, w := range weights {
			if !got[face][w] {
				t.Errorf("%s weight %v missing", face, w)
			}
		}
	}
}

func TestNewShaper(t *testing.T) {
	if s, err := NewShaper(); err != nil || s == nil {
		t.Fatalf("NewShaper: %v, %v", s, err)
	}
}

func TestMaterialUsesBrandPalette(t *testing.T) {
	for _, base := range []string{"dark", "light"} {
		th := theme.MustBuiltin(base)
		shaper, err := NewShaper()
		if err != nil {
			t.Fatal(err)
		}
		mt := Material(th, shaper)
		if mt.Shaper != shaper || mt.Face != Sans {
			t.Errorf("%s: shaper or face not set", base)
		}
		if mt.Bg != th.Colors.Bg1 || mt.Fg != th.Colors.Ink || mt.ContrastBg != th.Colors.Dial || mt.ContrastFg != th.Colors.OnDial {
			t.Errorf("%s: palette %+v does not follow the theme", base, mt.Palette)
		}
		if mt.TextSize != unit.Sp(13) || mt.FingerSize != unit.Dp(22) {
			t.Errorf("%s: text size %v, finger size %v", base, mt.TextSize, mt.FingerSize)
		}
	}
}

func TestFont(t *testing.T) {
	th := theme.MustBuiltin("dark")
	cases := []struct {
		style  string
		face   font.Typeface
		weight font.Weight
		size   unit.Sp
		ok     bool
	}{
		{"body", Sans, font.Normal, 13, true},
		{"body-strong", Sans, font.Medium, 13, true},
		{"title", Sans, font.SemiBold, 20, true},
		{"freq-xl", Mono, font.Medium, 32, true},
		{"readout", Mono, font.Normal, 11, true},
		{"no-such-style", Sans, font.Normal, 13, false},
	}
	for _, c := range cases {
		f, size, ok := Font(th, c.style)
		if f.Typeface != c.face || f.Weight != c.weight || size != c.size || ok != c.ok {
			t.Errorf("%s: got %v %v %v %v", c.style, f.Typeface, f.Weight, size, ok)
		}
	}
}

func TestLabel(t *testing.T) {
	th := theme.MustBuiltin("dark")
	shaper, err := NewShaper()
	if err != nil {
		t.Fatal(err)
	}
	mt := Material(th, shaper)
	red := color.NRGBA{R: 0xff, A: 0xff}
	l := Label(mt, th, "freq-xl", "145.500.000", red)
	if l.Font.Typeface != Mono || l.TextSize != 32 || l.LineHeight != 36 || l.Color != red || l.Text != "145.500.000" {
		t.Errorf("label: %+v", l)
	}
}

func TestDp(t *testing.T) {
	th := theme.MustBuiltin("dark")
	for token, want := range map[string]unit.Dp{
		"space-3": 12, "radius-md": 6, "control-md": 28, "sidebar-w": 288, "nope": 0,
	} {
		if got := Dp(th, token); got != want {
			t.Errorf("%s: %v, want %v", token, got, want)
		}
	}
}
