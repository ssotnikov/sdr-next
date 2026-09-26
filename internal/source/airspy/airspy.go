// Package airspy drives Airspy R2 and Mini receivers directly over USB,
// without libairspy. Request numbers follow the firmware's command table
// (airspy_commands.h in libairspy).
//
// The Airspy ADC samples a real IF at twice the output rate and streams raw
// 12-bit samples, so this driver does the real-to-I/Q conversion on the host,
// as libairspy does.
package airspy

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

const (
	vendorID  = 0x1d50
	productID = 0x60a1
)

// Vendor requests.
const (
	reqReceiverMode   = 1
	reqSetSampleRate  = 12
	reqSetFreq        = 13
	reqSetLNAGain     = 14
	reqSetMixerGain   = 15
	reqSetVGAGain     = 16
	reqGetSampleRates = 25
)

const (
	endpointRX   = 0x81
	transferSize = 128 << 10
	numTransfers = 8

	minFreq, maxFreq = 24e6, 1800e6

	// halfBandTaps sizes the real-to-I/Q filter.
	// TODO: replace with a proper half-band design (every other tap zero).
	halfBandTaps = 63
)

var gains = []source.GainStage{
	{Name: "LNA", Min: 0, Max: 14, Step: 1},
	{Name: "MIXER", Min: 0, Max: 15, Step: 1},
	{Name: "VGA", Min: 0, Max: 15, Step: 1},
}

func init() { source.Register(Driver{}) }

// Driver registers Airspy devices as "airspy". Source IDs are serial numbers.
type Driver struct{}

func (Driver) Name() string { return "airspy" }

func (Driver) Enumerate() ([]source.Info, error) {
	devs, err := usb.Enumerate(vendorID, productID)
	if err != nil {
		return nil, fmt.Errorf("airspy: %w", err)
	}
	infos := make([]source.Info, len(devs))
	for i, d := range devs {
		infos[i] = infoFor(d)
	}
	return infos, nil
}

func (Driver) Open(id string) (source.Source, error) {
	devs, err := usb.Enumerate(vendorID, productID)
	if err != nil {
		return nil, fmt.Errorf("airspy: %w", err)
	}
	for _, d := range devs {
		if id == "" || d.Serial == id {
			dev, err := usb.Open(d)
			if err != nil {
				return nil, fmt.Errorf("airspy: open %s: %w", d.Serial, err)
			}
			return newSource(dev, infoFor(d))
		}
	}
	if id == "" {
		return nil, errors.New("airspy: no device found")
	}
	return nil, fmt.Errorf("airspy: no device with serial %q", id)
}

func infoFor(d usb.DeviceInfo) source.Info {
	return source.Info{Driver: "airspy", ID: d.Serial, Label: "Airspy " + d.Serial}
}

// Source is an opened Airspy.
type Source struct {
	dev   usb.Device
	info  source.Info
	rates []float64 // supported output (I/Q) rates, as reported by the firmware

	mu   sync.Mutex
	rate float64
	freq float64
}

func newSource(dev usb.Device, info source.Info) (*Source, error) {
	rates, err := readSampleRates(dev)
	if err == nil && len(rates) == 0 {
		err = errors.New("device reports no sample rates")
	}
	s := &Source{dev: dev, info: info, rates: rates}
	if err == nil {
		err = s.SetSampleRate(rates[0])
	}
	if err != nil {
		dev.Close()
		return nil, fmt.Errorf("airspy: %w", err)
	}
	return s, nil
}

// readSampleRates asks for the rate count first, then for the list itself.
func readSampleRates(dev usb.Device) ([]float64, error) {
	var buf [4]byte
	if _, err := dev.VendorIn(reqGetSampleRates, 0, 0, buf[:]); err != nil {
		return nil, fmt.Errorf("get sample rate count: %w", err)
	}
	count := binary.LittleEndian.Uint32(buf[:])
	if count > 64 {
		return nil, fmt.Errorf("implausible sample rate count %d", count)
	}
	list := make([]byte, 4*count)
	n, err := dev.VendorIn(reqGetSampleRates, 0, uint16(count), list)
	if err != nil {
		return nil, fmt.Errorf("get sample rates: %w", err)
	}
	rates := make([]float64, n/4)
	for i := range rates {
		rates[i] = float64(binary.LittleEndian.Uint32(list[4*i:]))
	}
	return rates, nil
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

// SetSampleRate selects one of the rates the firmware reported.
func (s *Source) SetSampleRate(hz float64) error {
	idx := -1
	for i, r := range s.rates {
		if r == hz {
			idx = i
		}
	}
	if idx < 0 {
		return fmt.Errorf("airspy: unsupported sample rate %g (supported: %v)", hz, s.rates)
	}
	var resp [1]byte
	if _, err := s.dev.VendorIn(reqSetSampleRate, 0, uint16(idx), resp[:]); err != nil {
		return fmt.Errorf("airspy: set sample rate: %w", err)
	}
	s.mu.Lock()
	s.rate = hz
	s.mu.Unlock()
	return nil
}

// SetFrequency tunes the R820T.
//
// TODO: libairspy offsets the tuner frequency to account for where the IF
// lands after real-to-I/Q conversion. Verify the offset and spectrum
// orientation against hardware before relying on exact tuning.
func (s *Source) SetFrequency(hz float64) error {
	if hz < minFreq || hz > maxFreq {
		return fmt.Errorf("airspy: frequency %g out of range %g-%g", hz, minFreq, maxFreq)
	}
	var p [4]byte
	binary.LittleEndian.PutUint32(p[:], uint32(math.Round(hz)))
	if err := s.dev.VendorOut(reqSetFreq, 0, 0, p[:]); err != nil {
		return fmt.Errorf("airspy: set frequency: %w", err)
	}
	s.mu.Lock()
	s.freq = hz
	s.mu.Unlock()
	return nil
}

// SetGain sets a stage from Gains. Values are register indices, not dB.
func (s *Source) SetGain(stage string, value float64) error {
	var req uint8
	var maxValue float64
	switch strings.ToUpper(stage) {
	case "LNA":
		req, maxValue = reqSetLNAGain, 14
	case "MIXER":
		req, maxValue = reqSetMixerGain, 15
	case "VGA":
		req, maxValue = reqSetVGAGain, 15
	default:
		return fmt.Errorf("airspy: no gain stage %q", stage)
	}
	var resp [1]byte
	v := uint16(math.Round(min(max(value, 0), maxValue)))
	if _, err := s.dev.VendorIn(req, 0, v, resp[:]); err != nil {
		return fmt.Errorf("airspy: set %s: %w", stage, err)
	}
	return nil
}

// Stream converts the raw ADC stream to I/Q.
//
// Samples arrive as unpacked little-endian 16-bit words holding 12-bit
// unsigned values centered on 2048.
// TODO: validate scaling and orientation on hardware; add packed mode.
func (s *Source) Stream(ctx context.Context, fn func([]complex64) error) error {
	if err := s.dev.VendorOut(reqReceiverMode, 1, 0, nil); err != nil {
		return fmt.Errorf("airspy: start receive: %w", err)
	}
	conv := dsp.NewRealToIQ(halfBandTaps)
	var samples []float32
	var iq []complex64
	err := s.dev.BulkStream(ctx, endpointRX, transferSize, numTransfers, func(b []byte) error {
		n := len(b) / 2
		if cap(samples) < n {
			samples = make([]float32, n)
			iq = make([]complex64, n/2+1)
		}
		samples = samples[:n]
		for i := range samples {
			samples[i] = (float32(binary.LittleEndian.Uint16(b[2*i:])&0x0fff) - 2048) / 2048
		}
		return fn(iq[:conv.Process(samples, iq)])
	})
	if stopErr := s.dev.VendorOut(reqReceiverMode, 0, 0, nil); err == nil && stopErr != nil {
		err = fmt.Errorf("airspy: stop receive: %w", stopErr)
	}
	return err
}

var _ source.Source = (*Source)(nil)
