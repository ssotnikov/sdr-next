// Package iqfile plays back raw I/Q recordings. The sample format comes from
// the file extension; raw files carry no metadata, so the sample rate must be
// set to match the recording.
package iqfile

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/ssotnikov/sdr-next/internal/dsp"
	"github.com/ssotnikov/sdr-next/internal/source"
)

const blockSize = 1 << 14

// Format is an interleaved I/Q sample encoding.
type Format struct {
	Name    string
	size    int // bytes per complex sample
	convert func(dst []complex64, src []byte) int
}

var formats = map[string]Format{
	".cu8":   {"cu8", 2, dsp.CU8ToComplex},
	".cs8":   {"cs8", 2, dsp.CS8ToComplex},
	".cs16":  {"cs16", 4, dsp.CS16ToComplex},
	".cf32":  {"cf32", 8, dsp.CF32ToComplex},
	".cfile": {"cf32", 8, dsp.CF32ToComplex},
}

// FormatFor picks the sample format from a file name's extension.
func FormatFor(path string) (Format, error) {
	f, ok := formats[strings.ToLower(filepath.Ext(path))]
	if !ok {
		return Format{}, fmt.Errorf("iqfile: unknown format for %q (use .cu8, .cs8, .cs16, .cf32 or .cfile)", path)
	}
	return f, nil
}

func init() { source.Register(Driver{}) }

// Driver registers file playback as "file". The source ID is the file path.
type Driver struct{}

func (Driver) Name() string { return "file" }

// Enumerate returns nothing: files are named explicitly, not discovered.
func (Driver) Enumerate() ([]source.Info, error) { return nil, nil }

func (Driver) Open(path string) (source.Source, error) {
	if path == "" {
		return nil, errors.New("iqfile: a file path is required, e.g. file:recording.cs8")
	}
	format, err := FormatFor(path)
	if err != nil {
		return nil, err
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("iqfile: %w", err)
	}
	return &Source{f: f, format: format, rate: 2.4e6,
		info: source.Info{Driver: "file", ID: path, Label: format.Name + " recording " + filepath.Base(path)}}, nil
}

// Source reads samples from a file.
type Source struct {
	f      *os.File
	format Format
	info   source.Info
	rate   float64
	freq   float64
}

func (s *Source) Info() source.Info         { return s.info }
func (s *Source) SampleRate() float64       { return s.rate }
func (s *Source) Frequency() float64        { return s.freq }
func (s *Source) Gains() []source.GainStage { return nil }
func (s *Source) Close() error              { return s.f.Close() }

func (s *Source) SetSampleRate(hz float64) error {
	if hz <= 0 {
		return fmt.Errorf("iqfile: invalid sample rate %g", hz)
	}
	s.rate = hz
	return nil
}

// SetFrequency records the frequency the file was captured at. It cannot
// retune a recording.
func (s *Source) SetFrequency(hz float64) error {
	s.freq = hz
	return nil
}

func (s *Source) SetGain(stage string, value float64) error {
	return fmt.Errorf("iqfile: no gain stage %q", stage)
}

func (s *Source) Stream(ctx context.Context, fn func([]complex64) error) error {
	r := bufio.NewReaderSize(s.f, blockSize*s.format.size)
	raw := make([]byte, blockSize*s.format.size)
	iq := make([]complex64, blockSize)
	for ctx.Err() == nil {
		n, err := io.ReadFull(r, raw)
		if n > 0 {
			if ferr := fn(iq[:s.format.convert(iq, raw[:n])]); ferr != nil {
				return ferr
			}
		}
		switch {
		case err == io.EOF || err == io.ErrUnexpectedEOF:
			return nil
		case err != nil:
			return fmt.Errorf("iqfile: %w", err)
		}
	}
	return nil
}
