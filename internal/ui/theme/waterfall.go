package theme

import (
	"errors"
	"fmt"
	"image/color"
	"math"
)

// Stop is one waterfall palette stop at Pos in [0, 1].
type Stop struct {
	Pos   float32
	Color color.NRGBA
}

// Palette is a waterfall colour map (FR-VIS-6).
type Palette struct {
	ID    string
	Name  string
	Stops []Stop
}

func parsePalette(r *rawPalette) (Palette, error) {
	if !idRe.MatchString(r.ID) || len(r.Name) > 80 {
		return Palette{}, errors.New("theme: waterfall: invalid id or name")
	}
	if len(r.Stops) < 2 || len(r.Stops) > 64 {
		return Palette{}, errors.New("theme: waterfall: needs 2–64 stops")
	}
	p := Palette{ID: r.ID, Name: r.Name}
	prev := -1.0
	for i, s := range r.Stops {
		if math.IsNaN(s.Pos) || s.Pos < 0 || s.Pos > 1 || s.Pos <= prev {
			return Palette{}, fmt.Errorf("theme: waterfall stop %d: pos must increase within [0, 1]", i)
		}
		c, err := ParseColor(s.Color)
		if err != nil {
			return Palette{}, fmt.Errorf("theme: waterfall stop %d: %w", i, err)
		}
		prev = s.Pos
		p.Stops = append(p.Stops, Stop{Pos: float32(s.Pos), Color: c})
	}
	if p.Stops[0].Pos != 0 || p.Stops[len(p.Stops)-1].Pos != 1 {
		return Palette{}, errors.New("theme: waterfall stops must start at 0 and end at 1")
	}
	return p, nil
}

// At maps v in [0, 1] (clamped) to a colour by linear interpolation in sRGB.
func (p Palette) At(v float32) color.NRGBA {
	if len(p.Stops) == 0 {
		return color.NRGBA{A: 0xff}
	}
	if !(v > 0) { // also catches NaN
		return p.Stops[0].Color
	}
	if v >= 1 {
		return p.Stops[len(p.Stops)-1].Color
	}
	for i := 1; i < len(p.Stops); i++ {
		b := p.Stops[i]
		if v <= b.Pos {
			a := p.Stops[i-1]
			f := (v - a.Pos) / (b.Pos - a.Pos)
			l := func(x, y uint8) uint8 { return uint8(math.Round(float64(float32(x) + (float32(y)-float32(x))*f))) }
			return color.NRGBA{R: l(a.Color.R, b.Color.R), G: l(a.Color.G, b.Color.G), B: l(a.Color.B, b.Color.B), A: l(a.Color.A, b.Color.A)}
		}
	}
	return p.Stops[len(p.Stops)-1].Color
}

// LUT returns n colours sampled evenly from 0 to 1, for the waterfall
// renderer (index = quantised dB level). n is clamped to [2, 4096].
func (p Palette) LUT(n int) []color.NRGBA {
	if n < 2 {
		n = 2
	} else if n > 4096 {
		n = 4096
	}
	out := make([]color.NRGBA, n)
	for i := range out {
		out[i] = p.At(float32(i) / float32(n-1))
	}
	return out
}
