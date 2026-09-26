// Package theme loads SDR Next UI themes (JSON, format 1) and exposes them as
// typed values. It depends only on the standard library; package giotheme
// adapts a Theme to Gio's material.Theme and font collection.
//
// Built-in themes are generated from the SDR Next design system (tokens.json)
// and embedded; user themes (FR-UI-5) are parsed with strict limits (SEC-1):
// bounded input size, unknown fields rejected, every value validated. Values a
// user theme omits fall back to the built-in theme named by its "base".
package theme

import (
	"bytes"
	"embed"
	"encoding/json"
	"errors"
	"fmt"
	"image/color"
	"io"
	"regexp"
	"sort"
)

// Format is the only theme format version this package understands.
const Format = 1

// MaxThemeSize bounds a theme file (SEC-1).
const MaxThemeSize = 256 << 10

//go:embed themes/sdr-next-dark.json themes/sdr-next-light.json
var builtinFS embed.FS

// Colors are the semantic UI colours. Field comments name the design token.
type Colors struct {
	Bg0         color.NRGBA // bg-0: spectrum/waterfall ground, window
	Bg1         color.NRGBA // bg-1: sidebar, toolbar, status bar
	Bg2         color.NRGBA // bg-2: controls, cards
	Bg3         color.NRGBA // bg-3: hover, selected row
	Line        color.NRGBA // line: decorative separators, spectrum grid
	LineControl color.NRGBA // line-control: borders of interactive controls (≥3:1)
	Ink         color.NRGBA // ink: primary text
	InkMuted    color.NRGBA // ink-muted: labels, units
	InkFaint    color.NRGBA // ink-faint: leading zeros; not on Bg3
	Dial        color.NRGBA // dial: brand amber fill (primary action, active VFO)
	DialHover   color.NRGBA // dial-hover
	OnDial      color.NRGBA // on-dial: text on Dial
	DialInk     color.NRGBA // dial-ink: amber as text/line on surfaces
	DialSoft    color.NRGBA // dial-soft: tint behind DialInk
	OnVFO       color.NRGBA // on-vfo: label on a VFO colour fill
	Trace       color.NRGBA // trace: spectrum line, cursor
	TraceFill   color.NRGBA // trace-fill: area under the spectrum line
	Focus       color.NRGBA // focus: keyboard focus ring
	OK          color.NRGBA // ok: status "normal" (blue, colour-blind safe)
	Warn        color.NRGBA // warn
	Danger      color.NRGBA // danger
	Rec         color.NRGBA // rec: recording indicator only
	OKSoft      color.NRGBA // ok-soft
	WarnSoft    color.NRGBA // warn-soft
	DangerSoft  color.NRGBA // danger-soft
}

func (c *Colors) fields() map[string]*color.NRGBA {
	return map[string]*color.NRGBA{
		"bg0": &c.Bg0, "bg1": &c.Bg1, "bg2": &c.Bg2, "bg3": &c.Bg3, "line": &c.Line, "lineControl": &c.LineControl,
		"ink": &c.Ink, "inkMuted": &c.InkMuted, "inkFaint": &c.InkFaint,
		"dial": &c.Dial, "dialHover": &c.DialHover, "onDial": &c.OnDial, "dialInk": &c.DialInk, "dialSoft": &c.DialSoft,
		"onVfo": &c.OnVFO, "trace": &c.Trace, "traceFill": &c.TraceFill, "focus": &c.Focus,
		"ok": &c.OK, "warn": &c.Warn, "danger": &c.Danger, "rec": &c.Rec,
		"okSoft": &c.OKSoft, "warnSoft": &c.WarnSoft, "dangerSoft": &c.DangerSoft,
	}
}

// Font names the family and the font file per weight ("400", "500", …).
type Font struct {
	Family string            `json:"family"`
	Files  map[string]string `json:"files"`
}

// TextStyle is one step of the type scale. Size and LineHeight are in sp.
type TextStyle struct {
	Family     string  `json:"family"` // "sans" | "mono"
	Size       float32 `json:"size"`
	LineHeight float32 `json:"lineHeight"`
	Weight     int     `json:"weight"`
	Tracking   float32 `json:"tracking,omitempty"` // em
	Uppercase  bool    `json:"uppercase,omitempty"`
}

// Metrics hold spacing, radius and control sizes in dp, keyed by token name
// ("space-2", "radius-sm", "control-md", …).
type Metrics struct {
	Spacing map[string]float32 `json:"spacing"`
	Radius  map[string]float32 `json:"radius"`
	Size    map[string]float32 `json:"size"`
}

// FocusRing: a Gap of the surface colour, then Width of Color (a colour key).
type FocusRing struct {
	Gap   float32 `json:"gap"`
	Width float32 `json:"width"`
	Color string  `json:"color"`
}

// Shadow describes the floating-surface shadow (menus, dialogs).
type Shadow struct {
	Color   color.NRGBA
	OffsetY float32
	Blur    float32
}

// Theme is a fully resolved UI theme.
type Theme struct {
	ID          string
	Name        string
	Base        string // "dark" | "light"
	Colors      Colors
	VFO         []color.NRGBA
	Waterfall   Palette
	Fonts       map[string]Font
	Text        map[string]TextStyle
	Metrics     Metrics
	Opacity     map[string]float32
	FocusRing   FocusRing
	FloatShadow Shadow
}

// VFOColor returns the colour of VFO n (1-based); colours repeat cyclically.
func (t *Theme) VFOColor(n int) color.NRGBA {
	if len(t.VFO) == 0 {
		return t.Colors.Dial
	}
	if n < 1 {
		n = 1
	}
	return t.VFO[(n-1)%len(t.VFO)]
}

// Style returns the named text style and whether it exists.
func (t *Theme) Style(name string) (TextStyle, bool) {
	s, ok := t.Text[name]
	return s, ok
}

// Space returns a spacing token in dp (0 if unknown).
func (t *Theme) Space(name string) float32 { return t.Metrics.Spacing[name] }

// Radius returns a radius token in dp (0 if unknown).
func (t *Theme) Radius(name string) float32 { return t.Metrics.Radius[name] }

// FocusColor resolves FocusRing.Color against Colors.
func (t *Theme) FocusColor() color.NRGBA {
	if p, ok := t.Colors.fields()[t.FocusRing.Color]; ok {
		return *p
	}
	return t.Colors.Focus
}

// raw mirrors the JSON document.
type raw struct {
	Schema    string               `json:"$schema,omitempty"`
	Format    int                  `json:"format"`
	ID        string               `json:"id"`
	Name      string               `json:"name"`
	Base      string               `json:"base"`
	Colors    map[string]string    `json:"colors"`
	VFO       []string             `json:"vfo"`
	Waterfall *rawPalette          `json:"waterfall"`
	Fonts     map[string]Font      `json:"fonts"`
	Text      map[string]TextStyle `json:"text"`
	Metrics   *Metrics             `json:"metrics"`
	Opacity   map[string]float32   `json:"opacity"`
	FocusRing *FocusRing           `json:"focusRing"`
	Shadow    *struct {
		Color   string  `json:"color"`
		OffsetY float32 `json:"offsetY"`
		Blur    float32 `json:"blur"`
	} `json:"floatShadow"`
}

type rawPalette struct {
	ID    string `json:"id"`
	Name  string `json:"name"`
	Stops []struct {
		Pos   float64 `json:"pos"`
		Color string  `json:"color"`
	} `json:"stops"`
}

var (
	idRe       = regexp.MustCompile(`^[a-z0-9][a-z0-9-]{0,63}$`)
	fontFileRe = regexp.MustCompile(`^[A-Za-z0-9._-]{1,128}$`)
)

// ErrTooLarge is returned for theme documents over MaxThemeSize.
var ErrTooLarge = errors.New("theme: file too large")

// Builtin returns a built-in theme: "dark" (default) or "light".
func Builtin(base string) (*Theme, error) {
	if base != "light" {
		base = "dark"
	}
	b, err := builtinFS.ReadFile("themes/sdr-next-" + base + ".json")
	if err != nil {
		return nil, err
	}
	r, err := decode(bytes.NewReader(b))
	if err != nil {
		return nil, err
	}
	return resolve(r, nil)
}

// MustBuiltin is Builtin for package initialisation; it panics on error.
func MustBuiltin(base string) *Theme {
	t, err := Builtin(base)
	if err != nil {
		panic(err)
	}
	return t
}

// Load parses a user theme and fills anything it omits from the built-in
// theme of the same base.
func Load(r io.Reader) (*Theme, error) {
	doc, err := decode(r)
	if err != nil {
		return nil, err
	}
	base, err := Builtin(doc.Base)
	if err != nil {
		return nil, err
	}
	return resolve(doc, base)
}

func decode(r io.Reader) (*raw, error) {
	b, err := io.ReadAll(io.LimitReader(r, MaxThemeSize+1))
	if err != nil {
		return nil, fmt.Errorf("theme: read: %w", err)
	}
	if len(b) > MaxThemeSize {
		return nil, ErrTooLarge
	}
	dec := json.NewDecoder(bytes.NewReader(b))
	dec.DisallowUnknownFields()
	var doc raw
	if err := dec.Decode(&doc); err != nil {
		return nil, fmt.Errorf("theme: parse: %w", err)
	}
	if dec.More() {
		return nil, errors.New("theme: trailing data after JSON object")
	}
	if doc.Format != Format {
		return nil, fmt.Errorf("theme: unsupported format %d (want %d)", doc.Format, Format)
	}
	if !idRe.MatchString(doc.ID) {
		return nil, fmt.Errorf("theme: invalid id %q", doc.ID)
	}
	if len(doc.Name) > 80 {
		return nil, errors.New("theme: name longer than 80 bytes")
	}
	if doc.Base != "dark" && doc.Base != "light" {
		return nil, fmt.Errorf("theme: base must be \"dark\" or \"light\", got %q", doc.Base)
	}
	return &doc, nil
}

func resolve(doc *raw, base *Theme) (*Theme, error) {
	t := &Theme{ID: doc.ID, Name: doc.Name, Base: doc.Base}
	if base != nil {
		t.Colors = base.Colors
		t.VFO = append([]color.NRGBA(nil), base.VFO...)
		t.Waterfall = base.Waterfall
		t.Fonts = cloneMap(base.Fonts)
		t.Text = cloneMap(base.Text)
		t.Metrics = Metrics{cloneMap(base.Metrics.Spacing), cloneMap(base.Metrics.Radius), cloneMap(base.Metrics.Size)}
		t.Opacity = cloneMap(base.Opacity)
		t.FocusRing = base.FocusRing
		t.FloatShadow = base.FloatShadow
	} else {
		t.Fonts, t.Text, t.Opacity = map[string]Font{}, map[string]TextStyle{}, map[string]float32{}
		t.Metrics = Metrics{map[string]float32{}, map[string]float32{}, map[string]float32{}}
	}

	fields := t.Colors.fields()
	keys := make([]string, 0, len(doc.Colors))
	for k := range doc.Colors {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		p, ok := fields[k]
		if !ok {
			return nil, fmt.Errorf("theme: unknown colour %q", k)
		}
		c, err := ParseColor(doc.Colors[k])
		if err != nil {
			return nil, fmt.Errorf("theme: colour %q: %w", k, err)
		}
		*p = c
	}
	if base == nil && len(doc.Colors) != len(fields) {
		return nil, fmt.Errorf("theme: built-in theme defines %d of %d colours", len(doc.Colors), len(fields))
	}

	if doc.VFO != nil {
		if len(doc.VFO) == 0 || len(doc.VFO) > 16 {
			return nil, errors.New("theme: vfo needs 1–16 colours")
		}
		t.VFO = t.VFO[:0]
		for i, s := range doc.VFO {
			c, err := ParseColor(s)
			if err != nil {
				return nil, fmt.Errorf("theme: vfo[%d]: %w", i, err)
			}
			t.VFO = append(t.VFO, c)
		}
	}

	if doc.Waterfall != nil {
		p, err := parsePalette(doc.Waterfall)
		if err != nil {
			return nil, err
		}
		t.Waterfall = p
	}

	if len(doc.Fonts) > 4 {
		return nil, errors.New("theme: at most 4 font families")
	}
	for k, f := range doc.Fonts {
		if k != "sans" && k != "mono" {
			return nil, fmt.Errorf("theme: unknown font family key %q", k)
		}
		if len(f.Family) == 0 || len(f.Family) > 80 || len(f.Files) > 9 {
			return nil, fmt.Errorf("theme: font %q: invalid family or too many files", k)
		}
		for w, file := range f.Files {
			if !fontFileRe.MatchString(file) { // plain file names only (SEC-3)
				return nil, fmt.Errorf("theme: font %q weight %s: invalid file name %q", k, w, file)
			}
		}
		t.Fonts[k] = f
	}

	if len(doc.Text) > 32 {
		return nil, errors.New("theme: at most 32 text styles")
	}
	for k, s := range doc.Text {
		if !idRe.MatchString(k) {
			return nil, fmt.Errorf("theme: invalid text style name %q", k)
		}
		if s.Family != "sans" && s.Family != "mono" {
			return nil, fmt.Errorf("theme: text %q: family must be sans or mono", k)
		}
		if s.Size < 6 || s.Size > 96 || s.LineHeight < 6 || s.LineHeight > 128 || s.Weight < 100 || s.Weight > 900 ||
			s.Tracking < -0.2 || s.Tracking > 0.5 {
			return nil, fmt.Errorf("theme: text %q: value out of range", k)
		}
		t.Text[k] = s
	}

	if doc.Metrics != nil {
		for _, m := range []struct {
			src, dst map[string]float32
			name     string
		}{{doc.Metrics.Spacing, t.Metrics.Spacing, "spacing"}, {doc.Metrics.Radius, t.Metrics.Radius, "radius"}, {doc.Metrics.Size, t.Metrics.Size, "size"}} {
			if len(m.src) > 32 {
				return nil, fmt.Errorf("theme: too many %s tokens", m.name)
			}
			for k, v := range m.src {
				if !idRe.MatchString(k) || v < 0 || v > 4096 {
					return nil, fmt.Errorf("theme: %s %q invalid", m.name, k)
				}
				m.dst[k] = v
			}
		}
	}

	if len(doc.Opacity) > 16 {
		return nil, errors.New("theme: too many opacity tokens")
	}
	for k, v := range doc.Opacity {
		if !idRe.MatchString(k) || v < 0 || v > 1 {
			return nil, fmt.Errorf("theme: opacity %q invalid", k)
		}
		t.Opacity[k] = v
	}

	if doc.FocusRing != nil {
		fr := *doc.FocusRing
		if fr.Gap < 0 || fr.Gap > 8 || fr.Width < 1 || fr.Width > 8 {
			return nil, errors.New("theme: focusRing out of range")
		}
		if _, ok := fields[fr.Color]; !ok {
			return nil, fmt.Errorf("theme: focusRing.color %q is not a colour key", fr.Color)
		}
		t.FocusRing = fr
	}

	if doc.Shadow != nil {
		c, err := ParseColor(doc.Shadow.Color)
		if err != nil {
			return nil, fmt.Errorf("theme: floatShadow: %w", err)
		}
		if doc.Shadow.Blur < 0 || doc.Shadow.Blur > 64 || doc.Shadow.OffsetY < -64 || doc.Shadow.OffsetY > 64 {
			return nil, errors.New("theme: floatShadow out of range")
		}
		t.FloatShadow = Shadow{Color: c, OffsetY: doc.Shadow.OffsetY, Blur: doc.Shadow.Blur}
	}

	if len(t.VFO) == 0 || len(t.Waterfall.Stops) < 2 {
		return nil, errors.New("theme: vfo and waterfall are required")
	}
	return t, nil
}

func cloneMap[K comparable, V any](m map[K]V) map[K]V {
	out := make(map[K]V, len(m))
	for k, v := range m {
		out[k] = v
	}
	return out
}
