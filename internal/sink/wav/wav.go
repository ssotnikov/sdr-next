// Package wav writes mono 16-bit PCM WAV files.
package wav

import (
	"encoding/binary"
	"errors"
	"io"
	"math"
	"os"
)

const headerSize = 44

// Writer streams audio into a WAV file. The RIFF sizes are written as zero up
// front and patched on Close, so a file from an interrupted run still holds
// its audio.
type Writer struct {
	w      io.WriteSeeker
	closer io.Closer // the file opened by Create, if any
	rate   int
	bytes  int64 // audio bytes written
	buf    []byte
}

// Create creates the file at path and returns a Writer for it.
func Create(path string, sampleRate int) (*Writer, error) {
	f, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	w, err := NewWriter(f, sampleRate)
	if err != nil {
		f.Close()
		return nil, err
	}
	w.closer = f
	return w, nil
}

// NewWriter writes a WAV header to w. Close finalizes the header but does not
// close w.
func NewWriter(w io.WriteSeeker, sampleRate int) (*Writer, error) {
	if sampleRate <= 0 {
		return nil, errors.New("wav: invalid sample rate")
	}
	wr := &Writer{w: w, rate: sampleRate}
	if _, err := w.Write(wr.header()); err != nil {
		return nil, err
	}
	return wr, nil
}

func (w *Writer) header() []byte {
	h := make([]byte, 0, headerSize)
	h = append(h, "RIFF"...)
	h = binary.LittleEndian.AppendUint32(h, uint32(36+w.bytes))
	h = append(h, "WAVEfmt "...)
	h = binary.LittleEndian.AppendUint32(h, 16)             // fmt chunk size
	h = binary.LittleEndian.AppendUint16(h, 1)              // PCM
	h = binary.LittleEndian.AppendUint16(h, 1)              // channels
	h = binary.LittleEndian.AppendUint32(h, uint32(w.rate)) // sample rate
	h = binary.LittleEndian.AppendUint32(h, uint32(2*w.rate))
	h = binary.LittleEndian.AppendUint16(h, 2)  // block align
	h = binary.LittleEndian.AppendUint16(h, 16) // bits per sample
	h = append(h, "data"...)
	return binary.LittleEndian.AppendUint32(h, uint32(w.bytes))
}

// WriteAudio appends samples, clipping them to [-1, 1].
func (w *Writer) WriteAudio(samples []float32) error {
	w.buf = w.buf[:0]
	for _, s := range samples {
		v := int16(math.Round(float64(min(max(s, -1), 1)) * math.MaxInt16))
		w.buf = binary.LittleEndian.AppendUint16(w.buf, uint16(v))
	}
	n, err := w.w.Write(w.buf)
	w.bytes += int64(n)
	return err
}

// Close patches the header with the final sizes and closes the file if the
// Writer was made by Create.
func (w *Writer) Close() error {
	_, err := w.w.Seek(0, io.SeekStart)
	if err == nil {
		_, err = w.w.Write(w.header())
	}
	if w.closer != nil {
		err = errors.Join(err, w.closer.Close())
	}
	return err
}
