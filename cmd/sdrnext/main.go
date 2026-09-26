// Command sdrnext is the SDR Next receiver.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/signal"
	"strings"

	"github.com/ssotnikov/sdr-next/internal/receiver"
	"github.com/ssotnikov/sdr-next/internal/sink/wav"
	"github.com/ssotnikov/sdr-next/internal/source"

	// Source drivers register themselves on import.
	_ "github.com/ssotnikov/sdr-next/internal/source/airspy"
	_ "github.com/ssotnikov/sdr-next/internal/source/hackrf"
	_ "github.com/ssotnikov/sdr-next/internal/source/iqfile"
	_ "github.com/ssotnikov/sdr-next/internal/source/sim"
)

// version is set at build time with -ldflags "-X main.version=...".
var version = "dev"

const usage = `usage: sdrnext <command> [flags]

commands:
  devices   list available sources
  rx        receive and demodulate to a WAV file (sdrnext rx -h for flags)
  version   print the version
`

func main() {
	if len(os.Args) < 2 {
		fmt.Fprint(os.Stderr, usage)
		os.Exit(2)
	}
	var err error
	switch os.Args[1] {
	case "devices":
		listDevices(os.Stdout)
	case "rx":
		err = runRx(os.Args[2:])
	case "version":
		fmt.Println("sdrnext", version)
	case "help", "-h", "-help", "--help":
		fmt.Print(usage)
	default:
		fmt.Fprintf(os.Stderr, "sdrnext: unknown command %q\n\n%s", os.Args[1], usage)
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "sdrnext:", err)
		os.Exit(1)
	}
}

func listDevices(w io.Writer) {
	for _, d := range source.Drivers() {
		infos, err := d.Enumerate()
		if err != nil {
			fmt.Fprintf(w, "%-8s  unavailable: %v\n", d.Name(), err)
			continue
		}
		for _, in := range infos {
			fmt.Fprintf(w, "%-8s  %-20s  %s\n", in.Driver, in.ID, in.Label)
		}
	}
}

func runRx(args []string) error {
	fs := flag.NewFlagSet("rx", flag.ContinueOnError)
	device := fs.String("device", "sim", "source as `driver[:id]`, e.g. sim, hackrf, airspy:<serial>, file:rec.cs8")
	freq := hzValue(100e6)
	fs.Var(&freq, "freq", "center frequency, e.g. 100M, 145.5M, 433.92M")
	rate := hzValue(2.4e6)
	fs.Var(&rate, "rate", "sample rate, e.g. 2.4M, 10M")
	mode := fs.String("mode", "wfm", "demodulator: wfm or nfm")
	gain := fs.String("gain", "", "gain stages as `name=value,...`, e.g. LNA=16,VGA=20")
	out := fs.String("out", "", "output WAV `file` (required)")
	duration := fs.Duration("duration", 0, "stop after this long (0 runs until the source ends or Ctrl-C)")
	if err := fs.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return nil
		}
		return err
	}
	if *out == "" {
		return errors.New("rx: -out is required")
	}
	gains, err := parseGains(*gain)
	if err != nil {
		return err
	}

	driver, id, _ := strings.Cut(*device, ":")
	src, err := source.Open(driver, id)
	if err != nil {
		return err
	}
	defer src.Close()
	for _, g := range gains {
		if err := src.SetGain(g.name, g.value); err != nil {
			return err
		}
	}

	rx, err := receiver.New(src, receiver.Config{
		Frequency:  float64(freq),
		SampleRate: float64(rate),
		Mode:       receiver.Mode(*mode),
		AudioRate:  48e3,
	})
	if err != nil {
		return err
	}

	w, err := wav.Create(*out, int(math.Round(rx.AudioRate())))
	if err != nil {
		return err
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()
	if *duration > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, *duration)
		defer cancel()
	}

	fmt.Fprintf(os.Stderr, "%s: %s %s @ %s S/s -> %s (%.0f Hz audio)\n",
		src.Info().Label, *mode, &freq, &rate, *out, rx.AudioRate())
	return errors.Join(rx.Run(ctx, w), w.Close())
}
