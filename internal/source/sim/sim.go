// Package sim provides a synthetic source: one FM broadcast station carrying
// a steady tone, plus Gaussian noise. It exercises the whole receive chain
// without hardware and serves as the reference input for pipeline tests.
package sim

import (
	"context"
	"fmt"
	"math"
	"math/rand/v2"
	"sync/atomic"
	"time"

	"github.com/ssotnikov/sdr-next/internal/source"
)

const (
	// StationFrequency is where the simulated station transmits, in Hz.
	StationFrequency = 100e6

	blockSize = 1 << 14
)

var info = source.Info{Driver: "sim", ID: "fm", Label: "Simulated FM station at 100 MHz (1 kHz tone)"}

func init() { source.Register(Driver{}) }

// Driver registers the simulated source as "sim".
type Driver struct{}

func (Driver) Name() string                      { return info.Driver }
func (Driver) Enumerate() ([]source.Info, error) { return []source.Info{info}, nil }

func (Driver) Open(id string) (source.Source, error) {
	if id != "" && id != info.ID {
		return nil, fmt.Errorf("sim: unknown source %q", id)
	}
	return New(), nil
}

// Source is the simulated station. Configure its exported fields before
// calling Stream.
type Source struct {
	Tone      float64 // modulating tone, Hz
	Deviation float64 // peak FM deviation, Hz
	Noise     float64 // noise standard deviation per I/Q component
	Realtime  bool    // pace output to the sample rate instead of running flat out

	rate atomic.Uint64 // float64 bits
	freq atomic.Uint64 // float64 bits
}

// New returns a source tuned to the station at 2.4 MS/s, paced to real time.
func New() *Source {
	s := &Source{Tone: 1000, Deviation: 75e3, Noise: 0.01, Realtime: true}
	s.rate.Store(math.Float64bits(2.4e6))
	s.freq.Store(math.Float64bits(StationFrequency))
	return s
}

func (s *Source) Info() source.Info         { return info }
func (s *Source) SampleRate() float64       { return math.Float64frombits(s.rate.Load()) }
func (s *Source) Frequency() float64        { return math.Float64frombits(s.freq.Load()) }
func (s *Source) Gains() []source.GainStage { return nil }
func (s *Source) Close() error              { return nil }

func (s *Source) SetSampleRate(hz float64) error {
	if hz <= 0 {
		return fmt.Errorf("sim: invalid sample rate %g", hz)
	}
	s.rate.Store(math.Float64bits(hz))
	return nil
}

func (s *Source) SetFrequency(hz float64) error {
	s.freq.Store(math.Float64bits(hz))
	return nil
}

func (s *Source) SetGain(stage string, value float64) error {
	return fmt.Errorf("sim: no gain stage %q", stage)
}

func (s *Source) Stream(ctx context.Context, fn func([]complex64) error) error {
	buf := make([]complex64, blockSize)
	rng := rand.New(rand.NewPCG(1, 2))
	var carrier, tone float64 // phases, radians
	start := time.Now()
	var elapsed time.Duration // sample time emitted so far

	for ctx.Err() == nil {
		rate := s.SampleRate()
		offset := StationFrequency - s.Frequency()
		audible := math.Abs(offset)+s.Deviation < rate/2
		for i := range buf {
			var v complex64
			if audible {
				carrier += 2 * math.Pi * (offset + s.Deviation*math.Sin(tone)) / rate
				tone += 2 * math.Pi * s.Tone / rate
				v = complex(float32(0.5*math.Cos(carrier)), float32(0.5*math.Sin(carrier)))
			}
			buf[i] = v + complex(float32(rng.NormFloat64()*s.Noise), float32(rng.NormFloat64()*s.Noise))
		}
		carrier = math.Mod(carrier, 2*math.Pi)
		tone = math.Mod(tone, 2*math.Pi)

		if err := fn(buf); err != nil {
			return err
		}

		if s.Realtime {
			elapsed += time.Duration(float64(len(buf)) / rate * float64(time.Second))
			if err := sleepUntil(ctx, start.Add(elapsed)); err != nil {
				return nil
			}
		}
	}
	return nil
}

func sleepUntil(ctx context.Context, t time.Time) error {
	d := time.Until(t)
	if d <= 0 {
		return ctx.Err()
	}
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

var _ source.Source = (*Source)(nil)
