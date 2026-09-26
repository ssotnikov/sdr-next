package usb

// Planned backend: WinUSB called through golang.org/x/sys/windows, no cgo.
//
//   - enumerate: SetupAPI device-interface enumeration, matching VID/PID in
//     the hardware ID and reading the serial from the instance ID
//   - open: CreateFile with FILE_FLAG_OVERLAPPED, then WinUsb_Initialize
//   - vendor requests: WinUsb_ControlTransfer
//   - streaming: a ring of overlapped WinUsb_ReadPipe calls with RAW_IO set
//
// The device must be bound to the WinUSB driver, either by WCID descriptors in
// its firmware or manually with a tool such as Zadig.

func enumerate(vendorID, productID uint16) ([]DeviceInfo, error) {
	return nil, ErrNotImplemented
}

func open(info DeviceInfo) (Device, error) {
	return nil, ErrNotImplemented
}
