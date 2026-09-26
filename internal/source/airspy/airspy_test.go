package airspy

import (
	"context"
	"encoding/binary"
	"testing"

	"github.com/ssotnikov/sdr-next/internal/source"
	"github.com/ssotnikov/sdr-next/internal/usb/usbtest"
)

// newFake returns a device that reports the Airspy R2 rates.
func newFake() *usbtest.Fake {
	rates := []uint32{10000000, 2500000}
	return &usbtest.Fake{InFunc: func(req uint8, value, index uint16, data []byte) (int, error) {
		if req != reqGetSampleRates {
			data[0] = 1
			return len(data), nil
		}
		if index == 0 {
			binary.LittleEndian.PutUint32(data, uint32(len(rates)))
			return 4, nil
		}
		for i, r := range rates {
			binary.LittleEndian.PutUint32(data[4*i:], r)
		}
		return 4 * len(rates), nil
	}}
}

func open(t *testing.T, fake *usbtest.Fake) *Source {
	t.Helper()
	s, err := newSource(fake, source.Info{Driver: "airspy", ID: "test"})
	if err != nil {
		t.Fatal(err)
	}
	return s
}

func TestOpenReadsRatesAndSelectsFirst(t *testing.T) {
	s := open(t, newFake())
	if s.SampleRate() != 10e6 || len(s.rates) != 2 || s.rates[1] != 2.5e6 {
		t.Fatalf("rate %g, rates %v", s.SampleRate(), s.rates)
	}
}

func TestSetSampleRateUsesIndex(t *testing.T) {
	fake := newFake()
	s := open(t, fake)
	if err := s.SetSampleRate(2.5e6); err != nil {
		t.Fatal(err)
	}
	calls := fake.Calls()
	if c := calls[len(calls)-1]; c.Request != reqSetSampleRate || c.Index != 1 {
		t.Fatalf("got %+v, want rate index 1", c)
	}
	if s.SetSampleRate(3e6) == nil {
		t.Fatal("expected unsupported-rate error")
	}
}

func TestSetFrequencyEncodesHz(t *testing.T) {
	fake := newFake()
	s := open(t, fake)
	if err := s.SetFrequency(433.92e6); err != nil {
		t.Fatal(err)
	}
	calls := fake.Calls()
	c := calls[len(calls)-1]
	if c.Request != reqSetFreq || binary.LittleEndian.Uint32(c.Data) != 433920000 {
		t.Fatalf("got %+v", c)
	}
}

func TestStreamHalvesRate(t *testing.T) {
	raw := make([]byte, 2*1000)
	for i := range 1000 {
		binary.LittleEndian.PutUint16(raw[2*i:], 2048)
	}
	fake := newFake()
	fake.Transfers = [][]byte{raw}
	s := open(t, fake)
	var n int
	if err := s.Stream(context.Background(), func(iq []complex64) error { n += len(iq); return nil }); err != nil {
		t.Fatal(err)
	}
	if n != 500 {
		t.Fatalf("got %d I/Q samples from 1000 real, want 500", n)
	}
}
