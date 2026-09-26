//go:build !windows && !linux

package usb

func enumerate(vendorID, productID uint16) ([]DeviceInfo, error) {
	return nil, ErrNotImplemented
}

func open(info DeviceInfo) (Device, error) {
	return nil, ErrNotImplemented
}
