package main

import (
	"slices"
	"testing"
)

func TestParseHz(t *testing.T) {
	for in, want := range map[string]float64{
		"100M":     100e6,
		"145.5M":   145.5e6,
		"433.92M":  433.92e6,
		"2.4e6":    2.4e6,
		"12.5k":    12.5e3,
		"1.2G":     1.2e9,
		" 7000000": 7e6,
	} {
		got, err := parseHz(in)
		if err != nil || got != want {
			t.Errorf("parseHz(%q) = %v, %v; want %v", in, got, err, want)
		}
	}
	for _, in := range []string{"", "M", "abc", "-5M"} {
		if _, err := parseHz(in); err == nil {
			t.Errorf("parseHz(%q) succeeded, want error", in)
		}
	}
}

func TestHzValueString(t *testing.T) {
	v := hzValue(145.5e6)
	if s := v.String(); s != "145.5M" {
		t.Fatalf("got %q", s)
	}
}

func TestParseGains(t *testing.T) {
	got, err := parseGains("LNA=16, VGA = 20")
	want := []gainSetting{{"LNA", 16}, {"VGA", 20}}
	if err != nil || !slices.Equal(got, want) {
		t.Fatalf("got %v, %v", got, err)
	}
	if _, err := parseGains("LNA"); err == nil {
		t.Fatal("expected error for missing value")
	}
}
