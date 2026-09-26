// Package dsp contains the signal-processing blocks of the receiver.
//
// Blocks work on caller-provided buffers and carry their state across calls,
// so a stream can be processed in blocks of any size. They allocate only at
// construction or when an internal buffer must grow, which keeps the hot path
// free of GC pressure. Blocks are not safe for concurrent use.
package dsp

import "math"

// Lowpass designs a linear-phase low-pass FIR filter as a Blackman-windowed
// sinc. cutoff is the -6 dB point normalized to the sample rate
// (0 < cutoff < 0.5). The tap count is forced odd so the group delay is a whole
// number of samples, and the taps are normalized to unity gain at DC.
func Lowpass(numTaps int, cutoff float64) []float32 {
	numTaps = max(numTaps, 3) | 1
	h := make([]float64, numTaps)
	m := float64(numTaps - 1)
	var sum float64
	for i := range h {
		x := float64(i) - m/2
		s := 2 * cutoff
		if x != 0 {
			s = math.Sin(2*math.Pi*cutoff*x) / (math.Pi * x)
		}
		w := 0.42 - 0.5*math.Cos(2*math.Pi*float64(i)/m) + 0.08*math.Cos(4*math.Pi*float64(i)/m)
		h[i] = s * w
		sum += h[i]
	}
	taps := make([]float32, numTaps)
	for i, v := range h {
		taps[i] = float32(v / sum)
	}
	return taps
}

// TapsFor estimates how many Blackman-window taps a transition band of the
// given width (normalized to the sample rate) needs, clamped to a sane range.
func TapsFor(transition float64) int {
	n := int(math.Ceil(5.5 / transition))
	return min(max(n, 15), 1023) | 1
}

func reversed(taps []float32) []float32 {
	r := make([]float32, len(taps))
	for i, t := range taps {
		r[len(taps)-1-i] = t
	}
	return r
}

// grow returns s resized to n, reallocating only when the capacity is too small.
func grow[T any](s []T, n int) []T {
	if cap(s) < n {
		return make([]T, n)
	}
	return s[:n]
}
