// Package usb is a minimal, cgo-free USB host layer covering what SDR drivers
// need: enumeration by vendor/product ID, vendor control requests to the
// device, and continuous bulk IN streaming.
//
// Each platform has its own backend (WinUSB on Windows, usbfs on Linux) so the
// project builds with CGO_ENABLED=0 and cross-compiles to every target without
// a C toolchain or libusb.
package usb

import (
	"context"
	"errors"
	"runtime"
)

// ErrNotImplemented is returned by platform backends that are not written yet.
var ErrNotImplemented = errors.New("usb: backend not implemented on " + runtime.GOOS)

// DeviceInfo identifies an attached device.
type DeviceInfo struct {
	VendorID  uint16
	ProductID uint16
	Serial    string
	Path      string // platform-specific location used by Open
}

// Device is an opened USB device.
//
// Vendor requests always target the device recipient, matching how SDR
// firmware exposes its control interface.
type Device interface {
	// VendorIn issues a device-to-host vendor request and reads up to
	// len(data) bytes into data.
	VendorIn(request uint8, value, index uint16, data []byte) (int, error)

	// VendorOut issues a host-to-device vendor request carrying data.
	VendorOut(request uint8, value, index uint16, data []byte) error

	// BulkStream keeps count transfers of size bytes queued on the IN endpoint
	// and calls fn with each completed transfer, in order. data is only valid
	// until fn returns. It returns nil when ctx is cancelled, or the first
	// error from fn or the device.
	BulkStream(ctx context.Context, endpoint uint8, size, count int, fn func(data []byte) error) error

	Close() error
}

// Enumerate lists attached devices with the given vendor and product ID.
func Enumerate(vendorID, productID uint16) ([]DeviceInfo, error) {
	return enumerate(vendorID, productID)
}

// Open opens a device returned by Enumerate.
func Open(info DeviceInfo) (Device, error) {
	return open(info)
}
