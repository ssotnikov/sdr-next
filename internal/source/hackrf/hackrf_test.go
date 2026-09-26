package hackrf

import (
	"context"
	"encoding/binary"
	"testing"

	"github.com/ssotnikov/sdr-next/internal/source"
	"github.com/ssotnikov/sdr-next/internal/usb/usbtest"
)

func open(t *testing.T, fake *usbtest.Fake) *Source {
	t.Helper()
	s, err := newSource(fake, source.Info{Driver: "hackrf", ID: "test"})
	if err != nil {
		t.Fatal(err)
	}
	return s
}

func last(t *testing.T, fake *usbtest.Fake) usbtest.Call {
	t.Helper()
	calls := fake.Calls()
	if len(calls) == 0 {
		t.Fatal("no vendor requests issued")
	}
	return calls[len(calls)-1]
}

func TestSetFrequencySplitsMHzAndHz(t *testing.T) {
	fake := &usbtest.Fake{}
	s := open(t, fake)
	if err := s.SetFrequency(100.5e6); err != nil {
		t.Fatal(err)
	}
	c := last(t, fake)
	if c.Request != reqSetFreq || len(c.Data) != 8 {
		t.Fatalf("got %+v", c)
	}
	if mhz, hz := binary.LittleEndian.Uint32(c.Data), binary.LittleEndian.Uint32(c.Data[4:]); mhz != 100 || hz != 500000 {
		t.Fatalf("encoded %d MHz + %d Hz, want 100 MHz + 500000 Hz", mhz, hz)
	}
	if s.SetFrequency(8e9) == nil {
		t.Fatal("expected out-of-range error")
	}
}

func TestSetSampleRateAlsoSetsBasebandFilter(t *testing.T) {
	fake := &usbtest.Fake{}
	s := open(t, fake)
	if err := s.SetSampleRate(8e6); err != nil {
		t.Fatal(err)
	}
	calls := fake.Calls()
	rate, bw := calls[len(calls)-2], calls[len(calls)-1]
	if rate.Request != reqSampleRateSet || binary.LittleEndian.Uint32(rate.Data) != 8000000 || binary.LittleEndian.Uint32(rate.Data[4:]) != 1 {
		t.Fatalf("sample rate request %+v", rate)
	}
	// 75% of 8 MHz is 6 MHz, which is a supported filter width.
	if got := uint32(bw.Index)<<16 | uint32(bw.Value); bw.Request != reqBasebandFilterBandwidthSet || got != 6000000 {
		t.Fatalf("baseband filter %d Hz, want 6000000", got)
	}
}

func TestSetGainRoundsToStep(t *testing.T) {
	fake := &usbtest.Fake{}
	s := open(t, fake)
	if err := s.SetGain("lna", 23); err != nil {
		t.Fatal(err)
	}
	if c := last(t, fake); !c.In || c.Request != reqSetLNAGain || c.Index != 16 {
		t.Fatalf("got %+v, want LNA request with index 16", c)
	}

	fake.InFunc = func(uint8, uint16, uint16, []byte) (int, error) { return 1, nil } // leaves a zero byte: rejected
	if s.SetGain("VGA", 20) == nil {
		t.Fatal("expected rejection error")
	}
}

func TestStreamConvertsAndStopsReceiver(t *testing.T) {
	fake := &usbtest.Fake{Transfers: [][]byte{{0x40, 0xc0, 0x00, 0x7f}}}
	s := open(t, fake)
	var got []complex64
	err := s.Stream(context.Background(), func(iq []complex64) error {
		got = append(got, iq...)
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 2 || got[0] != complex(0.5, -0.5) || got[1] != complex(0, 127.0/128) {
		t.Fatalf("got %v", got)
	}
	calls := fake.Calls()
	start, stop := calls[len(calls)-2], calls[len(calls)-1]
	if start.Request != reqSetTransceiverMode || start.Value != modeReceive || stop.Request != reqSetTransceiverMode || stop.Value != modeOff {
		t.Fatalf("mode requests %+v, %+v", start, stop)
	}
}
