package dsp

import "math"

// FMDemod is a quadrature discriminator: each output is the phase change
// between consecutive input samples, scaled so a frequency offset equal to the
// peak deviation produces 1.0.
type FMDemod struct {
	prev complex64
	gain float32
}

// NewFMDemod returns an FM demodulator for input at sampleRate with the given
// peak deviation, both in Hz.
func NewFMDemod(sampleRate, deviation float64) *FMDemod {
	return &FMDemod{gain: float32(sampleRate / (2 * math.Pi * deviation))}
}

// Process demodulates in into out and returns len(in). out must be at least
// as long as in.
func (d *FMDemod) Process(in []complex64, out []float32) int {
	for i, s := range in {
		p := s * complex(real(d.prev), -imag(d.prev))
		out[i] = d.gain * float32(math.Atan2(float64(imag(p)), float64(real(p))))
		d.prev = s
	}
	return len(in)
}

// Deemphasis is the single-pole low-pass that undoes broadcast FM
// pre-emphasis (50 µs in most of the world, 75 µs in the Americas).
type Deemphasis struct {
	alpha, y float32
}

// NewDeemphasis returns a de-emphasis filter with time constant tau seconds
// for audio at sampleRate Hz.
func NewDeemphasis(sampleRate, tau float64) *Deemphasis {
	return &Deemphasis{alpha: float32(1 - math.Exp(-1/(tau*sampleRate)))}
}

// Process filters buf in place.
func (d *Deemphasis) Process(buf []float32) {
	for i, x := range buf {
		d.y += d.alpha * (x - d.y)
		buf[i] = d.y
	}
}
