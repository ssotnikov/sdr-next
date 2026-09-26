package theme

import (
	"bytes"
	"image/color"
	"strings"
	"testing"
)

func TestBuiltinThemesLoad(t *testing.T) {
	for _, base := range []string{"dark", "light"} {
		th, err := Builtin(base)
		if err != nil {
			t.Fatalf("%s: %v", base, err)
		}
		if th.Base != base || th.ID != "sdr-next-"+base {
			t.Errorf("%s: got id %q base %q", base, th.ID, th.Base)
		}
		if len(th.VFO) != 8 {
			t.Errorf("%s: want 8 VFO colours, got %d", base, len(th.VFO))
		}
		for _, s := range []string{"body", "heading", "label", "freq-xl", "readout", "code"} {
			if _, ok := th.Style(s); !ok {
				t.Errorf("%s: missing text style %q", base, s)
			}
		}
		if th.Space("space-2") != 8 || th.Radius("radius-sm") != 3 || th.Metrics.Size["control-md"] != 28 {
			t.Errorf("%s: metrics not loaded: %+v", base, th.Metrics)
		}
	}
}

func TestBuiltinDefaultsToDark(t *testing.T) {
	th, err := Builtin("")
	if err != nil || th.Base != "dark" {
		t.Fatalf("got %v, %v", th, err)
	}
}

// The brand book's contrast rules, enforced for every built-in theme.
func TestBuiltinContrast(t *testing.T) {
	type pair struct {
		name   string
		fg, bg func(*Theme) color.NRGBA
		min    float64
	}
	c := func(f func(*Colors) color.NRGBA) func(*Theme) color.NRGBA {
		return func(t *Theme) color.NRGBA { return f(&t.Colors) }
	}
	var pairs []pair
	surfaces := map[string]func(*Theme) color.NRGBA{
		"bg0": c(func(c *Colors) color.NRGBA { return c.Bg0 }), "bg1": c(func(c *Colors) color.NRGBA { return c.Bg1 }),
		"bg2": c(func(c *Colors) color.NRGBA { return c.Bg2 }), "bg3": c(func(c *Colors) color.NRGBA { return c.Bg3 }),
	}
	for sn, s := range surfaces {
		pairs = append(pairs,
			pair{"ink/" + sn, c(func(c *Colors) color.NRGBA { return c.Ink }), s, 4.5},
			pair{"inkMuted/" + sn, c(func(c *Colors) color.NRGBA { return c.InkMuted }), s, 4.5},
			pair{"dialInk/" + sn, c(func(c *Colors) color.NRGBA { return c.DialInk }), s, 4.5},
		)
		if sn != "bg3" {
			pairs = append(pairs, pair{"inkFaint/" + sn, c(func(c *Colors) color.NRGBA { return c.InkFaint }), s, 4.5},
				pair{"trace/" + sn, c(func(c *Colors) color.NRGBA { return c.Trace }), s, 3})
		}
	}
	pairs = append(pairs,
		pair{"onDial/dial", c(func(c *Colors) color.NRGBA { return c.OnDial }), c(func(c *Colors) color.NRGBA { return c.Dial }), 4.5},
		pair{"lineControl/bg1", c(func(c *Colors) color.NRGBA { return c.LineControl }), surfaces["bg1"], 3},
		pair{"ok/okSoft", c(func(c *Colors) color.NRGBA { return c.OK }), c(func(c *Colors) color.NRGBA { return c.OKSoft }), 4.5},
		pair{"warn/warnSoft", c(func(c *Colors) color.NRGBA { return c.Warn }), c(func(c *Colors) color.NRGBA { return c.WarnSoft }), 4.5},
		pair{"danger/dangerSoft", c(func(c *Colors) color.NRGBA { return c.Danger }), c(func(c *Colors) color.NRGBA { return c.DangerSoft }), 4.5},
	)
	for _, base := range []string{"dark", "light"} {
		th := MustBuiltin(base)
		for _, p := range pairs {
			if r := Contrast(p.fg(th), p.bg(th)); r < p.min {
				t.Errorf("%s %s: contrast %.2f < %.1f", base, p.name, r, p.min)
			}
		}
		for i := 1; i <= 8; i++ {
			if r := Contrast(th.Colors.OnVFO, th.VFOColor(i)); r < 4.5 {
				t.Errorf("%s onVfo/vfo-%d: %.2f < 4.5", base, i, r)
			}
			if r := Contrast(th.VFOColor(i), th.Colors.Bg0); r < 3 {
				t.Errorf("%s vfo-%d/bg0: %.2f < 3", base, i, r)
			}
		}
	}
}

func TestVFOColorCycles(t *testing.T) {
	th := MustBuiltin("dark")
	if th.VFOColor(9) != th.VFOColor(1) || th.VFOColor(0) != th.VFOColor(1) {
		t.Error("VFO colours must repeat from VFO 9 and clamp below 1")
	}
}

func TestFocusColorIsTrace(t *testing.T) {
	for _, base := range []string{"dark", "light"} {
		th := MustBuiltin(base)
		if th.FocusColor() != th.Colors.Trace {
			t.Errorf("%s: focus ring must be the trace colour", base)
		}
	}
}

func TestLoadUserThemeOverridesAndFallsBack(t *testing.T) {
	src := `{"format":1,"id":"my-amber","name":"Моя","base":"dark",
		"colors":{"dial":"#FFAA00"},
		"vfo":["#ff0000","#00ff00"],
		"waterfall":{"id":"grey","stops":[{"pos":0,"color":"#000000"},{"pos":1,"color":"#ffffff"}]}}`
	th, err := Load(strings.NewReader(src))
	if err != nil {
		t.Fatal(err)
	}
	if th.Colors.Dial != (color.NRGBA{0xff, 0xaa, 0x00, 0xff}) {
		t.Errorf("dial not overridden: %v", th.Colors.Dial)
	}
	if th.Colors.Ink != MustBuiltin("dark").Colors.Ink {
		t.Error("ink must fall back to the built-in dark theme")
	}
	if th.VFOColor(3) != th.VFOColor(1) {
		t.Error("2-colour VFO palette must cycle")
	}
	if _, ok := th.Style("body"); !ok {
		t.Error("text styles must fall back")
	}
	if got := th.Waterfall.At(0.5); got != (color.NRGBA{0x80, 0x80, 0x80, 0xff}) {
		t.Errorf("grey midpoint: %v", got)
	}
}

func TestLoadDoesNotMutateBuiltin(t *testing.T) {
	_, err := Load(strings.NewReader(`{"format":1,"id":"x","name":"x","base":"light","metrics":{"spacing":{"space-2":10}}}`))
	if err != nil {
		t.Fatal(err)
	}
	if MustBuiltin("light").Space("space-2") != 8 {
		t.Error("user theme leaked into the built-in theme")
	}
}

func TestLoadRejects(t *testing.T) {
	cases := map[string]string{
		"unknown field":   `{"format":1,"id":"x","name":"x","base":"dark","evil":1}`,
		"unknown colour":  `{"format":1,"id":"x","name":"x","base":"dark","colors":{"hotpink":"#ff00ff"}}`,
		"bad colour":      `{"format":1,"id":"x","name":"x","base":"dark","colors":{"ink":"red"}}`,
		"bad format":      `{"format":2,"id":"x","name":"x","base":"dark"}`,
		"bad base":        `{"format":1,"id":"x","name":"x","base":"sepia"}`,
		"bad id":          `{"format":1,"id":"../x","name":"x","base":"dark"}`,
		"path in font":    `{"format":1,"id":"x","name":"x","base":"dark","fonts":{"sans":{"family":"X","files":{"400":"../../etc/passwd"}}}}`,
		"stops order":     `{"format":1,"id":"x","name":"x","base":"dark","waterfall":{"id":"w","stops":[{"pos":0.5,"color":"#000000"},{"pos":0.2,"color":"#ffffff"}]}}`,
		"stops ends":      `{"format":1,"id":"x","name":"x","base":"dark","waterfall":{"id":"w","stops":[{"pos":0.1,"color":"#000000"},{"pos":1,"color":"#ffffff"}]}}`,
		"empty vfo":       `{"format":1,"id":"x","name":"x","base":"dark","vfo":[]}`,
		"huge text":       `{"format":1,"id":"x","name":"x","base":"dark","text":{"body":{"family":"sans","size":500,"lineHeight":20,"weight":400}}}`,
		"focus key":       `{"format":1,"id":"x","name":"x","base":"dark","focusRing":{"gap":2,"width":2,"color":"nope"}}`,
		"trailing":        `{"format":1,"id":"x","name":"x","base":"dark"} {}`,
		"negative metric": `{"format":1,"id":"x","name":"x","base":"dark","metrics":{"radius":{"radius-sm":-1}}}`,
	}
	for name, src := range cases {
		if _, err := Load(strings.NewReader(src)); err == nil {
			t.Errorf("%s: expected error", name)
		}
	}
}

func TestLoadTooLarge(t *testing.T) {
	big := bytes.Repeat([]byte(" "), MaxThemeSize+1)
	if _, err := Load(bytes.NewReader(big)); err != ErrTooLarge {
		t.Fatalf("want ErrTooLarge, got %v", err)
	}
}

func TestParseColorAndHex(t *testing.T) {
	c, err := ParseColor("#8FD8FF33")
	if err != nil || c != (color.NRGBA{0x8f, 0xd8, 0xff, 0x33}) {
		t.Fatalf("%v %v", c, err)
	}
	if Hex(c) != "#8fd8ff33" {
		t.Errorf("Hex: %s", Hex(c))
	}
	for _, bad := range []string{"", "#", "#fff", "8fd8ff", "#8fd8fg", "#8fd8ff3"} {
		if _, err := ParseColor(bad); err == nil {
			t.Errorf("%q: expected error", bad)
		}
	}
}

func TestWaterfallLUT(t *testing.T) {
	p := MustBuiltin("dark").Waterfall
	lut := p.LUT(256)
	if len(lut) != 256 || lut[0] != p.Stops[0].Color || lut[255] != p.Stops[len(p.Stops)-1].Color {
		t.Fatal("LUT endpoints must equal the first and last stops")
	}
	if p.At(-1) != lut[0] || p.At(2) != lut[255] {
		t.Error("At must clamp")
	}
	var nan float32
	nan = nan / nan
	if p.At(nan) != lut[0] {
		t.Error("At(NaN) must return the first stop")
	}
	if len(p.LUT(0)) != 2 || len(p.LUT(1<<20)) != 4096 {
		t.Error("LUT size must be clamped")
	}
}

func TestOverAndWithAlpha(t *testing.T) {
	th := MustBuiltin("dark")
	band := WithAlpha(th.VFOColor(1), th.Opacity["vfo-band"])
	if band.A != 46 { // 255 × 0.18
		t.Errorf("vfo-band alpha: %d", band.A)
	}
	if got := Over(color.NRGBA{255, 255, 255, 128}, color.NRGBA{0, 0, 0, 255}); got.R != 128 || got.A != 255 {
		t.Errorf("Over: %v", got)
	}
}

func FuzzLoad(f *testing.F) {
	f.Add([]byte(`{"format":1,"id":"x","name":"x","base":"dark","colors":{"ink":"#ffffff"}}`))
	f.Add([]byte(`{"format":1,"id":"x","name":"x","base":"light","waterfall":{"id":"w","stops":[{"pos":0,"color":"#000"},{"pos":1,"color":"#fff"}]}}`))
	f.Fuzz(func(t *testing.T, b []byte) {
		th, err := Load(bytes.NewReader(b))
		if err == nil && (len(th.VFO) == 0 || len(th.Waterfall.Stops) < 2) {
			t.Fatal("accepted theme without VFO colours or waterfall")
		}
	})
}
