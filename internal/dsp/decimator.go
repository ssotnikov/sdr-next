package dsp

// Decimator low-pass filters complex samples and keeps every factor-th output.
// Only the retained outputs are computed.
type Decimator struct {
	taps   []float32   // reversed, so the inner loop is a plain dot product
	factor int         // keep every factor-th output
	buf    []complex64 // len(taps)-1 samples of history, then the current block
	skip   int         // input samples to skip before the next output
}

// NewDecimator returns a decimator that applies taps and then keeps every
// factor-th sample.
func NewDecimator(taps []float32, factor int) *Decimator {
	if factor < 1 {
		panic("dsp: decimation factor must be >= 1")
	}
	return &Decimator{taps: reversed(taps), factor: factor, buf: make([]complex64, len(taps)-1)}
}

// Factor returns the decimation factor.
func (d *Decimator) Factor() int { return d.factor }

// Process filters in and writes the decimated result to out, returning the
// number of samples written. out must have room for len(in)/factor+1 samples.
func (d *Decimator) Process(in, out []complex64) int {
	nt := len(d.taps)
	d.buf = append(d.buf[:nt-1], in...)
	n := 0
	i := d.skip
	for ; i < len(in); i += d.factor {
		w := d.buf[i : i+nt]
		var re, im float32
		for k, t := range d.taps {
			re += real(w[k]) * t
			im += imag(w[k]) * t
		}
		out[n] = complex(re, im)
		n++
	}
	d.skip = i - len(in)
	d.buf = d.buf[:copy(d.buf, d.buf[len(in):])]
	return n
}

// RealDecimator is the float32 counterpart of Decimator, used for audio.
type RealDecimator struct {
	taps   []float32
	factor int
	buf    []float32
	skip   int
}

// NewRealDecimator returns a decimator that applies taps and then keeps every
// factor-th sample.
func NewRealDecimator(taps []float32, factor int) *RealDecimator {
	if factor < 1 {
		panic("dsp: decimation factor must be >= 1")
	}
	return &RealDecimator{taps: reversed(taps), factor: factor, buf: make([]float32, len(taps)-1)}
}

// Factor returns the decimation factor.
func (d *RealDecimator) Factor() int { return d.factor }

// Process filters in and writes the decimated result to out, returning the
// number of samples written. out must have room for len(in)/factor+1 samples.
func (d *RealDecimator) Process(in, out []float32) int {
	nt := len(d.taps)
	d.buf = append(d.buf[:nt-1], in...)
	n := 0
	i := d.skip
	for ; i < len(in); i += d.factor {
		w := d.buf[i : i+nt]
		var acc float32
		for k, t := range d.taps {
			acc += w[k] * t
		}
		out[n] = acc
		n++
	}
	d.skip = i - len(in)
	d.buf = d.buf[:copy(d.buf, d.buf[len(in):])]
	return n
}
