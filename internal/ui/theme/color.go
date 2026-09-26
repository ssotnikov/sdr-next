package theme

import (
	"errors"
	"image/color"
	"math"
)

var errColor = errors.New("colour must be #rrggbb or #rrggbbaa")

// ParseColor parses "#rrggbb" or "#rrggbbaa" (case-insensitive).
func ParseColor(s string) (color.NRGBA, error) {
	if (len(s) != 7 && len(s) != 9) || s[0] != '#' {
		return color.NRGBA{}, errColor
	}
	var v [4]uint8
	v[3] = 0xff
	for i := 0; i < (len(s)-1)/2; i++ {
		hi, ok1 := hexVal(s[1+2*i])
		lo, ok2 := hexVal(s[2+2*i])
		if !ok1 || !ok2 {
			return color.NRGBA{}, errColor
		}
		v[i] = hi<<4 | lo
	}
	return color.NRGBA{R: v[0], G: v[1], B: v[2], A: v[3]}, nil
}

// Hex formats c as "#rrggbbaa".
func Hex(c color.NRGBA) string {
	const d = "0123456789abcdef"
	b := []byte{'#', 0, 0, 0, 0, 0, 0, 0, 0}
	for i, x := range []uint8{c.R, c.G, c.B, c.A} {
		b[1+2*i], b[2+2*i] = d[x>>4], d[x&0xf]
	}
	return string(b)
}

func hexVal(c byte) (uint8, bool) {
	switch {
	case c >= '0' && c <= '9':
		return c - '0', true
	case c >= 'a' && c <= 'f':
		return c - 'a' + 10, true
	case c >= 'A' && c <= 'F':
		return c - 'A' + 10, true
	}
	return 0, false
}

// Over composites fg over an opaque bg (sRGB, as the UI renders it).
func Over(fg, bg color.NRGBA) color.NRGBA {
	a := float64(fg.A) / 255
	mix := func(f, b uint8) uint8 { return uint8(math.Round(float64(f)*a + float64(b)*(1-a))) }
	return color.NRGBA{R: mix(fg.R, bg.R), G: mix(fg.G, bg.G), B: mix(fg.B, bg.B), A: 0xff}
}

// WithAlpha returns c with its alpha scaled by f (0…1), e.g. for vfo-band.
func WithAlpha(c color.NRGBA, f float32) color.NRGBA {
	if f < 0 {
		f = 0
	} else if f > 1 {
		f = 1
	}
	c.A = uint8(math.Round(float64(c.A) * float64(f)))
	return c
}

// Contrast returns the WCAG 2 contrast ratio between two opaque colours.
func Contrast(a, b color.NRGBA) float64 {
	la, lb := luminance(a), luminance(b)
	if la < lb {
		la, lb = lb, la
	}
	return (la + 0.05) / (lb + 0.05)
}

func luminance(c color.NRGBA) float64 {
	lin := func(v uint8) float64 {
		x := float64(v) / 255
		if x <= 0.03928 {
			return x / 12.92
		}
		return math.Pow((x+0.055)/1.055, 2.4)
	}
	return 0.2126*lin(c.R) + 0.7152*lin(c.G) + 0.0722*lin(c.B)
}
