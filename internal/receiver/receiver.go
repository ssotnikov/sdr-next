// Package receiver wires a source to a demodulation chain and an audio sink.
//
// The chain for FM is:
//
//	source → channel filter + decimate to IF rate → FM discriminator
//	       → de-emphasis → audio filter + decimate → sink
//
// The source runs in its own goroutine and hands blocks to the DSP goroutine
// through a small bounded queue, so the USB callback is never blocked on DSP
// except when the queue is full.
package receiver

import (
	"context"
	"fmt"
	"math"

	"github.com/ssotnikov/sdr-next/internal/dsp"
	"github.com/ssotnikov/sdr-next/internal/source"
)

// Mode selects the demodulator.
type Mode string

const (
	WFM Mode = "wfm" // broadcast FM
	NFM Mode = "nfm" // narrowband FM (voice)
)

type modeParams struct {
	ifRate      float64 // target rate after the channel filter
	channelBW   float64 // two-sided channel bandwidth kept by the channel filter
	deviation   float64 // peak deviation, normalizes audio to ±1
	audioCutoff float64
	deemphasis  float64 // time constant in seconds, 0 for none
}

var modes = map[Mode]modeParams{
	// TODO: make de-emphasis selectable (75 µs in the Americas).
	WFM: {ifRate: 250e3, channelBW: 200e3, deviation: 75e3, audioCutoff: 15e3, deemphasis: 50e-6},
	NFM: {ifRate: 50e3, channelBW: 12.5e3, deviation: 2.5e3, audioCutoff: 3.5e3},
}

// queueDepth is how many source blocks may wait for the DSP goroutine.
const queueDepth = 8

// Config describes what to receive.
type Config struct {
	Frequency  float64 // Hz
	SampleRate float64 // Hz; must be supported by the source
	Mode       Mode
	AudioRate  float64 // desired audio rate; see Receiver.AudioRate for the actual one
}

// AudioWriter consumes demodulated mono audio in the range [-1, 1].
type AudioWriter interface {
	WriteAudio(samples []float32) error
}

// Receiver demodulates one channel from a source.
type Receiver struct {
	src       source.Source
	audioRate float64

	chanDec  *dsp.Decimator // nil when no decimation is needed
	demod    *dsp.FMDemod
	deemph   *dsp.Deemphasis
	audioDec *dsp.RealDecimator

	ifBuf, audioOut []float32
	ifIQ            []complex64
}

// New configures src and builds the demodulation chain.
//
// Decimation factors are whole numbers chosen to land near the mode's IF rate
// and the requested audio rate, so the actual audio rate can differ from
// Config.AudioRate. TODO: add a rational resampler for exact audio rates.
func New(src source.Source, cfg Config) (*Receiver, error) {
	p, ok := modes[cfg.Mode]
	if !ok {
		return nil, fmt.Errorf("receiver: unknown mode %q", cfg.Mode)
	}
	if cfg.AudioRate <= 0 {
		return nil, fmt.Errorf("receiver: invalid audio rate %g", cfg.AudioRate)
	}
	if err := src.SetSampleRate(cfg.SampleRate); err != nil {
		return nil, err
	}
	if err := src.SetFrequency(cfg.Frequency); err != nil {
		return nil, err
	}

	fs := cfg.SampleRate
	d1 := max(1, int(math.Round(fs/p.ifRate)))
	ifRate := fs / float64(d1)
	d2 := max(1, int(math.Round(ifRate/cfg.AudioRate)))
	audioRate := ifRate / float64(d2)

	r := &Receiver{
		src:       src,
		audioRate: audioRate,
		demod:     dsp.NewFMDemod(ifRate, p.deviation),
	}
	if d1 > 1 {
		r.chanDec = dsp.NewDecimator(decimationFilter(fs, ifRate, p.channelBW/2), d1)
	}
	if p.deemphasis > 0 {
		r.deemph = dsp.NewDeemphasis(ifRate, p.deemphasis)
	}
	if d2 > 1 {
		r.audioDec = dsp.NewRealDecimator(decimationFilter(ifRate, audioRate, p.audioCutoff), d2)
	}
	return r, nil
}

// decimationFilter designs the anti-alias filter for going from inRate to
// outRate while keeping passband Hz intact. Its -6 dB point sits at the output
// Nyquist rate; the stopband starts where aliases would fold into the passband.
func decimationFilter(inRate, outRate, passband float64) []float32 {
	passband = min(passband, 0.45*outRate)
	transition := outRate - 2*passband
	return dsp.Lowpass(dsp.TapsFor(transition/inRate), 0.5*outRate/inRate)
}

// AudioRate is the rate of the audio passed to the AudioWriter, in Hz.
func (r *Receiver) AudioRate() float64 { return r.audioRate }

// Tune retunes the source while running.
func (r *Receiver) Tune(hz float64) error { return r.src.SetFrequency(hz) }

// Run streams from the source and writes audio to out until ctx is cancelled
// (returns nil), the source ends (nil), or the source or out fails.
func (r *Receiver) Run(ctx context.Context, out AudioWriter) error {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	blocks := make(chan []complex64, queueDepth)
	free := make(chan []complex64, queueDepth+1)
	srcErr := make(chan error, 1)

	go func() {
		defer close(blocks)
		srcErr <- r.src.Stream(ctx, func(iq []complex64) error {
			var buf []complex64
			select {
			case buf = <-free:
			default:
			}
			buf = append(buf[:0], iq...)
			select {
			case blocks <- buf:
				return nil
			case <-ctx.Done():
				return ctx.Err()
			}
		})
	}()

	var err error
	for buf := range blocks {
		if err == nil {
			if err = r.process(buf, out); err != nil {
				cancel()
			}
		}
		select {
		case free <- buf:
		default:
		}
	}
	// Source errors after cancellation are just the shutdown echoing back.
	if serr := <-srcErr; err == nil && ctx.Err() == nil {
		err = serr
	}
	return err
}

func (r *Receiver) process(iq []complex64, out AudioWriter) error {
	if r.chanDec != nil {
		r.ifIQ = grow(r.ifIQ, len(iq)/r.chanDec.Factor()+1)
		iq = r.ifIQ[:r.chanDec.Process(iq, r.ifIQ)]
	}
	r.ifBuf = grow(r.ifBuf, len(iq))
	audio := r.ifBuf[:r.demod.Process(iq, r.ifBuf)]
	if r.deemph != nil {
		r.deemph.Process(audio)
	}
	if r.audioDec != nil {
		r.audioOut = grow(r.audioOut, len(audio)/r.audioDec.Factor()+1)
		audio = r.audioOut[:r.audioDec.Process(audio, r.audioOut)]
	}
	return out.WriteAudio(audio)
}

func grow[T any](s []T, n int) []T {
	if cap(s) < n {
		return make([]T, n)
	}
	return s[:n]
}
