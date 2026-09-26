package main

import (
	"fmt"
	"strconv"
	"strings"
)

// hzValue is a flag.Value for frequencies and rates that accepts k, M and G
// suffixes.
type hzValue float64

func (h *hzValue) String() string {
	v := float64(*h)
	for _, u := range []struct {
		suffix string
		scale  float64
	}{{"G", 1e9}, {"M", 1e6}, {"k", 1e3}} {
		if v >= u.scale {
			return strconv.FormatFloat(v/u.scale, 'f', -1, 64) + u.suffix
		}
	}
	return strconv.FormatFloat(v, 'f', -1, 64)
}

func (h *hzValue) Set(s string) error {
	v, err := parseHz(s)
	if err != nil {
		return err
	}
	*h = hzValue(v)
	return nil
}

func parseHz(s string) (float64, error) {
	s = strings.TrimSpace(s)
	scale := 1.0
	if n := len(s); n > 0 {
		switch s[n-1] {
		case 'k', 'K':
			scale = 1e3
		case 'M':
			scale = 1e6
		case 'G', 'g':
			scale = 1e9
		}
		if scale != 1 {
			s = s[:n-1]
		}
	}
	v, err := strconv.ParseFloat(s, 64)
	if err != nil || v < 0 {
		return 0, fmt.Errorf("invalid frequency %q", s)
	}
	return v * scale, nil
}

type gainSetting struct {
	name  string
	value float64
}

// parseGains parses "LNA=16,VGA=20" into ordered settings.
func parseGains(s string) ([]gainSetting, error) {
	if s == "" {
		return nil, nil
	}
	var gains []gainSetting
	for _, part := range strings.Split(s, ",") {
		name, val, ok := strings.Cut(part, "=")
		if !ok {
			return nil, fmt.Errorf("invalid gain %q, want name=value", part)
		}
		v, err := strconv.ParseFloat(strings.TrimSpace(val), 64)
		if err != nil {
			return nil, fmt.Errorf("invalid gain value in %q", part)
		}
		gains = append(gains, gainSetting{strings.TrimSpace(name), v})
	}
	return gains, nil
}
