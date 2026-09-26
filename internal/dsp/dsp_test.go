package dsp

import (
	"math"
	"math/cmplx"
	"testing"
)

func tone(n int, freq, rate float64) []complex64 {
	s := make([]complex64, n)
	for i := range s {
		s[i] = complex64(cmplx.Rect(1, 2*math.Pi*freq*float64(i)/rate))
	}
	return s
}

func rms(s []complex64) float64 {
	var sum float64
	for _, v := range s {
		sum += float64(real(v)*real(v) + imag(v)*imag(v))
	}
	return math.Sqrt(sum / float64(len(s)))
}

// meanFreq returns the average frequency of a complex tone in Hz.
func meanFreq(s []complex64, rate float64) float64 {
	var acc complex128
	for i := 1; i < len(s); i++ {
		acc += complex128(s[i] * complex(real(s[i-1]), -imag(s[i-1])))
	}
	return cmplx.Phase(acc) * rate / (2 * math.Pi)
}

func TestLowpassUnityDCAndSymmetric(t *testing.T) {
	taps := Lowpass(64, 0.1)
	if len(taps)%2 != 1 {
		t.Fatalf("got %d taps, want odd", len(taps))
	}
	var sum float32
	for i, v := range taps {
		sum += v
		if d := v - taps[len(taps)-1-i]; d > 1e-7 || d < -1e-7 {
			t.Fatalf("taps not symmetric at %d", i)
		}
	}
	if math.Abs(float64(sum)-1) > 1e-5 {
		t.Fatalf("DC gain %v, want 1", sum)
	}
}

func TestDecimatorBlockSizeIndependent(t *testing.T) {
	in := tone(5000, 1234, 48000)
	taps := Lowpass(41, 0.1)

	whole := make([]complex64, len(in)/3+1)
	n := NewDecimator(taps, 3).Process(in, whole)
	whole = whole[:n]

	d := NewDecimator(taps, 3)
	var chunked []complex64
	out := make([]complex64, len(in))
	for off, size := 0, 1; off < len(in); off, size = off+size, size%97+1 {
		end := min(off+size, len(in))
		chunked = append(chunked, out[:d.Process(in[off:end], out)]...)
	}

	if len(chunked) != len(whole) {
		t.Fatalf("chunked produced %d samples, whole produced %d", len(chunked), len(whole))
	}
	for i := range whole {
		if cmplx.Abs(complex128(whole[i]-chunked[i])) > 1e-5 {
			t.Fatalf("sample %d differs: %v vs %v", i, whole[i], chunked[i])
		}
	}
}

func TestDecimatorPassesAndRejects(t *testing.T) {
	const rate = 1e6
	taps := Lowpass(TapsFor(0.05), 0.1)
	check := func(freq float64) float64 {
		in := tone(20000, freq, rate)
		out := make([]complex64, len(in)/5+1)
		out = out[:NewDecimator(taps, 5).Process(in, out)]
		return rms(out[len(taps):]) // skip the filter's warm-up
	}
	if g := check(20e3); g < 0.99 || g > 1.01 {
		t.Errorf("passband gain %.3f, want ~1", g)
	}
	if g := check(300e3); g > 1e-3 {
		t.Errorf("stopband gain %.2g, want < 1e-3", g)
	}
}

func TestFMDemodScalesByDeviation(t *testing.T) {
	const rate, dev = 250e3, 75e3
	out := make([]float32, 1000)
	NewFMDemod(rate, dev).Process(tone(1000, 37.5e3, rate), out)
	for i, v := range out[1:] {
		if math.Abs(float64(v)-0.5) > 1e-3 {
			t.Fatalf("sample %d = %v, want 0.5", i+1, v)
		}
	}
}

func TestRealToIQShiftsQuarterRate(t *testing.T) {
	const rate = 1000.0
	in := make([]float32, 8000)
	for i := range in {
		in[i] = float32(math.Cos(2 * math.Pi * 300 * float64(i) / rate))
	}
	out := make([]complex64, len(in)/2+1)
	out = out[:NewRealToIQ(63).Process(in, out)]
	// A real tone at 300 Hz should appear at 300 - rate/4 = +50 Hz at rate/2.
	if f := meanFreq(out[100:], rate/2); math.Abs(f-50) > 0.5 {
		t.Fatalf("tone at %.2f Hz, want 50 Hz", f)
	}
}

func TestCS8ToComplex(t *testing.T) {
	dst := make([]complex64, 4)
	n := CS8ToComplex(dst, []byte{0x7f, 0x80, 0x00, 0x40, 0xff})
	if n != 2 {
		t.Fatalf("converted %d samples, want 2", n)
	}
	if dst[0] != complex(127.0/128, -1) || dst[1] != complex(0, 0.5) {
		t.Fatalf("got %v", dst[:n])
	}
}
