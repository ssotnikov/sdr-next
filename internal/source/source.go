// Package source defines the I/Q sample source abstraction and the registry
// through which drivers (hardware, simulated, file) make themselves available.
//
// Drivers register from an init function; a binary opts into a driver by
// importing its package, usually as a blank import from main.
package source

import (
	"context"
	"fmt"
	"slices"
	"strings"
	"sync"
)

// Info describes a source a driver can open.
type Info struct {
	Driver string // registry name, e.g. "hackrf"
	ID     string // driver-specific identifier passed to Driver.Open, e.g. a serial number
	Label  string // human-readable description
}

// GainStage describes one adjustable gain element in the device's own units.
// Stages are exposed individually rather than folded into a single dB figure
// because most SDR front ends are not calibrated in dB.
type GainStage struct {
	Name           string
	Min, Max, Step float64
}

// Source produces complex baseband samples.
//
// Setters may be called while Stream is running to retune or adjust the
// device. A Source is not otherwise safe for concurrent use.
type Source interface {
	Info() Info

	SampleRate() float64
	SetSampleRate(hz float64) error

	Frequency() float64
	SetFrequency(hz float64) error

	Gains() []GainStage
	SetGain(stage string, value float64) error

	// Stream delivers blocks of samples to fn until ctx is cancelled (it then
	// returns nil), the source is exhausted (nil), or fn or the device returns
	// an error. The block passed to fn is reused once fn returns.
	Stream(ctx context.Context, fn func(iq []complex64) error) error

	Close() error
}

// Driver discovers and opens sources of one kind.
type Driver interface {
	Name() string
	Enumerate() ([]Info, error)
	// Open opens the source with the given ID; an empty ID selects the first
	// available one.
	Open(id string) (Source, error)
}

var (
	mu      sync.RWMutex
	drivers = map[string]Driver{}
)

// Register makes a driver available by name. It panics if the name is taken.
func Register(d Driver) {
	mu.Lock()
	defer mu.Unlock()
	if _, dup := drivers[d.Name()]; dup {
		panic("source: driver registered twice: " + d.Name())
	}
	drivers[d.Name()] = d
}

// Drivers returns the registered drivers sorted by name.
func Drivers() []Driver {
	mu.RLock()
	defer mu.RUnlock()
	ds := make([]Driver, 0, len(drivers))
	for _, d := range drivers {
		ds = append(ds, d)
	}
	slices.SortFunc(ds, func(a, b Driver) int { return strings.Compare(a.Name(), b.Name()) })
	return ds
}

// Open opens a source through the named driver.
func Open(driver, id string) (Source, error) {
	mu.RLock()
	d, ok := drivers[driver]
	mu.RUnlock()
	if !ok {
		var names []string
		for _, d := range Drivers() {
			names = append(names, d.Name())
		}
		return nil, fmt.Errorf("unknown source driver %q (available: %s)", driver, strings.Join(names, ", "))
	}
	return d.Open(id)
}
