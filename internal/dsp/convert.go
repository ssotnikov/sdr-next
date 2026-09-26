package dsp

import (
	"encoding/binary"
	"math"
)

// The converters below turn raw interleaved I/Q bytes into complex64 samples
// scaled to roughly [-1, 1). Each writes min(len(dst), len(src)/size) samples
// and returns that count.

// CS8ToComplex converts signed 8-bit I/Q (HackRF native format).
func CS8ToComplex(dst []complex64, src []byte) int {
	n := min(len(dst), len(src)/2)
	for i := range n {
		dst[i] = complex(float32(int8(src[2*i]))/128, float32(int8(src[2*i+1]))/128)
	}
	return n
}

// CU8ToComplex converts unsigned 8-bit I/Q centered on 127.5 (RTL-SDR format).
func CU8ToComplex(dst []complex64, src []byte) int {
	n := min(len(dst), len(src)/2)
	for i := range n {
		dst[i] = complex((float32(src[2*i])-127.5)/128, (float32(src[2*i+1])-127.5)/128)
	}
	return n
}

// CS16ToComplex converts signed little-endian 16-bit I/Q.
func CS16ToComplex(dst []complex64, src []byte) int {
	n := min(len(dst), len(src)/4)
	for i := range n {
		re := int16(binary.LittleEndian.Uint16(src[4*i:]))
		im := int16(binary.LittleEndian.Uint16(src[4*i+2:]))
		dst[i] = complex(float32(re)/32768, float32(im)/32768)
	}
	return n
}

// CF32ToComplex converts little-endian float32 I/Q (GNU Radio .cfile format).
func CF32ToComplex(dst []complex64, src []byte) int {
	n := min(len(dst), len(src)/8)
	for i := range n {
		re := math.Float32frombits(binary.LittleEndian.Uint32(src[8*i:]))
		im := math.Float32frombits(binary.LittleEndian.Uint32(src[8*i+4:]))
		dst[i] = complex(re, im)
	}
	return n
}

// RealToIQ turns a real-valued stream sampled at rate R into complex baseband
// at R/2. It removes DC, shifts the spectrum down by R/4 so the band 0..R/2
// lands on -R/4..R/4, then low-pass filters and decimates by two. Receivers
// that sample a real IF, such as the Airspy, need this step.
type RealToIQ struct {
	dec   *Decimator
	phase int     // position in the 4-step e^(-jπn/2) mixer sequence
	dc    float32 // running DC estimate
	mixed []complex64
}

// NewRealToIQ returns a converter whose half-band filter has numTaps taps.
func NewRealToIQ(numTaps int) *RealToIQ {
	return &RealToIQ{dec: NewDecimator(Lowpass(numTaps, 0.25), 2)}
}

// Process converts in and writes complex samples to out, returning the number
// written. out must have room for len(in)/2+1 samples.
func (c *RealToIQ) Process(in []float32, out []complex64) int {
	const dcAlpha = 1e-4
	c.mixed = grow(c.mixed, len(in))
	for i, x := range in {
		c.dc += dcAlpha * (x - c.dc)
		x -= c.dc
		switch c.phase {
		case 0:
			c.mixed[i] = complex(x, 0)
		case 1:
			c.mixed[i] = complex(0, -x)
		case 2:
			c.mixed[i] = complex(-x, 0)
		case 3:
			c.mixed[i] = complex(0, x)
		}
		c.phase = (c.phase + 1) & 3
	}
	return c.dec.Process(c.mixed, out)
}
