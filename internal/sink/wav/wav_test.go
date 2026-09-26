package wav

import (
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func TestHeaderPatchedOnClose(t *testing.T) {
	path := filepath.Join(t.TempDir(), "out.wav")
	w, err := Create(path, 48000)
	if err != nil {
		t.Fatal(err)
	}
	if err := w.WriteAudio([]float32{0, 1, -1, 2}); err != nil {
		t.Fatal(err)
	}
	if err := w.Close(); err != nil {
		t.Fatal(err)
	}

	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if len(b) != headerSize+8 {
		t.Fatalf("file is %d bytes, want %d", len(b), headerSize+8)
	}
	le := binary.LittleEndian
	if string(b[0:4]) != "RIFF" || le.Uint32(b[4:]) != 36+8 || string(b[8:16]) != "WAVEfmt " {
		t.Fatalf("bad RIFF header % x", b[:16])
	}
	if le.Uint32(b[24:]) != 48000 || le.Uint32(b[40:]) != 8 {
		t.Fatalf("rate %d, data size %d", le.Uint32(b[24:]), le.Uint32(b[40:]))
	}
	want := []int16{0, 32767, -32767, 32767} // last sample clipped
	for i, v := range want {
		if got := int16(le.Uint16(b[headerSize+2*i:])); got != v {
			t.Errorf("sample %d = %d, want %d", i, got, v)
		}
	}
}
