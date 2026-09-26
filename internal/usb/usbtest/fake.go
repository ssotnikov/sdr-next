// Package usbtest provides a scripted usb.Device for driver tests.
package usbtest

import (
	"context"
	"slices"
	"sync"
)

// Call records one vendor request.
type Call struct {
	In      bool
	Request uint8
	Value   uint16
	Index   uint16
	Data    []byte // payload sent (out) or buffer size requested (in, zeroed)
}

// Fake is an in-memory usb.Device.
type Fake struct {
	// InFunc answers VendorIn. When nil, every byte of data is set to 1,
	// which drivers treat as success.
	InFunc func(request uint8, value, index uint16, data []byte) (int, error)

	// Transfers are delivered in order by BulkStream, which then returns nil.
	Transfers [][]byte

	mu     sync.Mutex
	calls  []Call
	closed bool
}

// Calls returns the vendor requests issued so far.
func (f *Fake) Calls() []Call {
	f.mu.Lock()
	defer f.mu.Unlock()
	return slices.Clone(f.calls)
}

// Closed reports whether Close was called.
func (f *Fake) Closed() bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.closed
}

func (f *Fake) record(c Call) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, c)
}

func (f *Fake) VendorIn(request uint8, value, index uint16, data []byte) (int, error) {
	f.record(Call{In: true, Request: request, Value: value, Index: index, Data: make([]byte, len(data))})
	if f.InFunc != nil {
		return f.InFunc(request, value, index, data)
	}
	for i := range data {
		data[i] = 1
	}
	return len(data), nil
}

func (f *Fake) VendorOut(request uint8, value, index uint16, data []byte) error {
	f.record(Call{Request: request, Value: value, Index: index, Data: slices.Clone(data)})
	return nil
}

func (f *Fake) BulkStream(ctx context.Context, endpoint uint8, size, count int, fn func([]byte) error) error {
	for _, t := range f.Transfers {
		if ctx.Err() != nil {
			return nil
		}
		if err := fn(t); err != nil {
			return err
		}
	}
	return nil
}

func (f *Fake) Close() error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.closed = true
	return nil
}
