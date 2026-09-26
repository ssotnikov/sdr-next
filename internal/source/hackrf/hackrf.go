// Package hackrf drives HackRF devices directly over USB, without libhackrf.
// Request numbers and payload layouts follow the HackRF firmware's vendor
// request table (hackrf_vendor_request in libhackrf's hackrf.c).
package hackrf

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"strings"
	"sync"

	"github.com/ssotnikov/sdr-next/internal/dsp"
	"github.com/ssotnikov/sdr-next/internal/source"
	"github.com/ssotnikov/sdr-next/internal/usb"
)

const vendorID = 0x1d50

var products = []struct {
	id   uint16
	name string
}{
	{0x6089, "HackRF One"},
	{0x604b, "HackRF Jawbreaker"},
	{0xcc15, "rad1o"},
}

// Vendor requests.
const (
	reqSetTransceiverMode         = 1
	reqSampleRateSet              = 6
	reqBasebandFilterBandwidthSet = 7
	reqSetFreq                    = 16
	reqAmpEnable                  = 17
	reqSetLNAGain                 = 19
	reqSetVGAGain                 = 20
)

// Transceiver modes.
const (
	modeOff     = 0
	modeReceive = 1
)

const (
	endpointRX   = 0x81
	transferSize = 256 << 10
	numTransfers = 4

	minFreq, maxFreq = 1e6, 7250e6
	minRate, maxRate = 2e6, 20e6
)

// Baseband filter bandwidths supported by the MAX2837, in Hz.
var basebandFilters = []uint32{
	1750000, 2500000, 3500000, 5000000, 5500000, 6000000, 7000000, 8000000,
	9000000, 10000000, 12000000, 14000000, 15000000, 20000000, 24000000, 28000000,
}

var gains = []source.GainStage{
	{Name: "AMP", Min: 0, Max: 14, Step: 14}, // RF amplifier, on or off
	{Name: "LNA", Min: 0, Max: 40, Step: 8},  // IF gain, dB
	{Name: "VGA", Min: 0, Max: 62, Step: 2},  // baseband gain, dB
}

func init() { source.Register(Driver{}) }

// Driver registers HackRF devices as "hackrf". Source IDs are serial numbers.
type Driver struct{}

func (Driver) Name() string { return "hackrf" }

func (Driver) Enumerate() ([]source.Info, error) {
	infos, _, err := enumerate()
	return infos, err
}

func (Driver) Open(id string) (source.Source, error) {
	infos, devs, err := enumerate()
	if err != nil {
		return nil, err
	}
	for i, in := range infos {
		if id == "" || in.ID == id {
			dev, err := usb.Open(devs[i])
			if err != nil {
				return nil, fmt.Errorf("hackrf: open %s: %w", in.ID, err)
			}
			return newSource(dev, in)
		}
	}
	if id == "" {
		return nil, errors.New("hackrf: no device found")
	}
	return nil, fmt.Errorf("hackrf: no device with serial %q", id)
}

func enumerate() ([]source.Info, []usb.DeviceInfo, error) {
	var infos []source.Info
	var devs []usb.DeviceInfo
	for _, p := range products {
		found, err := usb.Enumerate(vendorID, p.id)
		if err != nil {
			return nil, nil, fmt.Errorf("hackrf: %w", err)
		}
		for _, d := range found {
			infos = append(infos, source.Info{Driver: "hackrf", ID: d.Serial, Label: p.name + " " + d.Serial})
			devs = append(devs, d)
		}
	}
	return infos, devs, nil
}

// Source is an opened HackRF.
type Source struct {
	dev  usb.Device
	info source.Info

	mu   sync.Mutex
	rate float64
	freq float64
}

func newSource(dev usb.Device, info source.Info) (*Source, error) {
	s := &Source{dev: dev, info: info}
	if err := s.SetSampleRate(10e6); err != nil {
		dev.Close()
		return nil, err
	}
	return s, nil
}

func (s *Source) Info() source.Info         { return s.info }
func (s *Source) Gains() []source.GainStage { return gains }
func (s *Source) Close() error              { return s.dev.Close() }

func (s *Source) SampleRate() float64 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.rate
}

func (s *Source) Frequency() float64 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.freq
}

// SetSampleRate sets an integer sample rate and the widest baseband filter
// that fits within 75% of it.
func (s *Source) SetSampleRate(hz float64) error {
	if hz < minRate || hz > maxRate {
		return fmt.Errorf("hackrf: sample rate %g out of range %g-%g", hz, minRate, maxRate)
	}
	var p [8]byte
	binary.LittleEndian.PutUint32(p[0:], uint32(math.Round(hz)))
	binary.LittleEndian.PutUint32(p[4:], 1) // divider
	if err := s.dev.VendorOut(reqSampleRateSet, 0, 0, p[:]); err != nil {
		return fmt.Errorf("hackrf: set sample rate: %w", err)
	}
	bw := basebandFilter(0.75 * hz)
	if err := s.dev.VendorOut(reqBasebandFilterBandwidthSet, uint16(bw), uint16(bw>>16), nil); err != nil {
		return fmt.Errorf("hackrf: set baseband filter: %w", err)
	}
	s.mu.Lock()
	s.rate = hz
	s.mu.Unlock()
	return nil
}

func basebandFilter(hz float64) uint32 {
	bw := basebandFilters[0]
	for _, f := range basebandFilters {
		if float64(f) <= hz {
			bw = f
		}
	}
	return bw
}

func (s *Source) SetFrequency(hz float64) error {
	if hz < minFreq || hz > maxFreq {
		return fmt.Errorf("hackrf: frequency %g out of range %g-%g", hz, minFreq, maxFreq)
	}
	f := uint64(math.Round(hz))
	var p [8]byte
	binary.LittleEndian.PutUint32(p[0:], uint32(f/1e6))
	binary.LittleEndian.PutUint32(p[4:], uint32(f%1e6))
	if err := s.dev.VendorOut(reqSetFreq, 0, 0, p[:]); err != nil {
		return fmt.Errorf("hackrf: set frequency: %w", err)
	}
	s.mu.Lock()
	s.freq = hz
	s.mu.Unlock()
	return nil
}

// SetGain sets a stage from Gains. Values are rounded down to the stage's step.
func (s *Source) SetGain(stage string, value float64) error {
	switch strings.ToUpper(stage) {
	case "AMP":
		var on uint16
		if value > 0 {
			on = 1
		}
		if err := s.dev.VendorOut(reqAmpEnable, on, 0, nil); err != nil {
			return fmt.Errorf("hackrf: set AMP: %w", err)
		}
		return nil
	case "LNA":
		return s.setGainIn(reqSetLNAGain, "LNA", value, 40, 8)
	case "VGA":
		return s.setGainIn(reqSetVGAGain, "VGA", value, 62, 2)
	}
	return fmt.Errorf("hackrf: no gain stage %q", stage)
}

// setGainIn issues a gain request; the firmware answers with one byte that is
// zero when it rejected the value.
func (s *Source) setGainIn(req uint8, name string, value, maxValue, step float64) error {
	v := uint16(math.Floor(min(max(value, 0), maxValue)/step) * step)
	var resp [1]byte
	n, err := s.dev.VendorIn(req, 0, v, resp[:])
	if err != nil {
		return fmt.Errorf("hackrf: set %s: %w", name, err)
	}
	if n != 1 || resp[0] == 0 {
		return fmt.Errorf("hackrf: device rejected %s gain %d", name, v)
	}
	return nil
}

func (s *Source) Stream(ctx context.Context, fn func([]complex64) error) error {
	if err := s.dev.VendorOut(reqSetTransceiverMode, modeReceive, 0, nil); err != nil {
		return fmt.Errorf("hackrf: start receive: %w", err)
	}
	var iq []complex64
	err := s.dev.BulkStream(ctx, endpointRX, transferSize, numTransfers, func(b []byte) error {
		if cap(iq) < len(b)/2 {
			iq = make([]complex64, len(b)/2)
		}
		return fn(iq[:dsp.CS8ToComplex(iq[:cap(iq)], b)])
	})
	if stopErr := s.dev.VendorOut(reqSetTransceiverMode, modeOff, 0, nil); err == nil && stopErr != nil {
		err = fmt.Errorf("hackrf: stop receive: %w", stopErr)
	}
	return err
}

var _ source.Source = (*Source)(nil)
