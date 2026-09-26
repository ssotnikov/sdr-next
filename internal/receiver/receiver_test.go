package receiver

import (
	"context"
	"math"
	"testing"

	"github.com/ssotnikov/sdr-next/internal/source/sim"
)

// capture collects audio and cancels the run once it has enough.
type capture struct {
	want   int
	cancel context.CancelFunc
	audio  []float32
}

func (c *capture) WriteAudio(s []float32) error {
	c.audio = append(c.audio, s...)
	if len(c.audio) >= c.want {
		c.cancel()
	}
	return nil
}

func TestWFMRecoversSimulatedTone(t *testing.T) {
	src := sim.New()
	src.Realtime = false
	rx, err := New(src, Config{Frequency: sim.StationFrequency, SampleRate: 2.4e6, Mode: WFM, AudioRate: 48e3})
	if err != nil {
		t.Fatal(err)
	}
	if rx.AudioRate() != 48e3 {
		t.Fatalf("audio rate %g, want 48000", rx.AudioRate())
	}

	ctx, cancel := context.WithCancel(context.Background())
	c := &capture{want: 48000, cancel: cancel}
	if err := rx.Run(ctx, c); err != nil {
		t.Fatal(err)
	}

	audio := c.audio[4800:] // skip filter warm-up
	var crossings int
	var peak float64
	for i := 1; i < len(audio); i++ {
		if (audio[i-1] < 0) != (audio[i] < 0) {
			crossings++
		}
		peak = max(peak, math.Abs(float64(audio[i])))
	}
	freq := float64(crossings) / 2 / (float64(len(audio)) / rx.AudioRate())
	if math.Abs(freq-sim.New().Tone) > 10 {
		t.Errorf("recovered tone at %.1f Hz, want 1000 Hz", freq)
	}
	if peak < 0.7 || peak > 1.1 {
		t.Errorf("peak level %.2f, want about 1 for full deviation", peak)
	}
}

func TestUnknownModeRejected(t *testing.T) {
	if _, err := New(sim.New(), Config{SampleRate: 2.4e6, Mode: "am", AudioRate: 48e3}); err == nil {
		t.Fatal("expected error")
	}
}
