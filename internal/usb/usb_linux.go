package usb

// Planned backend: usbfs through golang.org/x/sys/unix, no cgo.
//
//   - enumerate: walk /sys/bus/usb/devices, reading idVendor, idProduct,
//     serial, busnum and devnum
//   - open: /dev/bus/usb/BBB/DDD, then USBDEVFS_CLAIMINTERFACE
//   - vendor requests: USBDEVFS_CONTROL
//   - streaming: a ring of USBDEVFS_SUBMITURB bulk URBs reaped with
//     USBDEVFS_REAPURB
//
// Non-root access needs udev rules granting the user access to the device
// (the airspy and hackrf projects ship suitable ones).

func enumerate(vendorID, productID uint16) ([]DeviceInfo, error) {
	return nil, ErrNotImplemented
}

func open(info DeviceInfo) (Device, error) {
	return nil, ErrNotImplemented
}
