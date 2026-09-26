# Build, test and check SDR Next on Linux (tested on WSL Ubuntu).
#
# All builds use CGO_ENABLED=0 (ADR-0002); `race` is the only target that
# needs cgo and a C compiler (sudo apt install build-essential).
#
#   make            check + build for the host architecture
#   make build      build for the host architecture
#   make linux      build linux/amd64 and linux/arm64
#   make cross      build every supported OS and architecture
#   make test       run the tests
#   make race       run the tests with the race detector
#   make vet        run go vet
#   make fmt        fail if any file is not gofmt-formatted
#   make vuln       run govulncheck (must be installed)
#   make check      fmt + vet + test
#   make clean      remove dist/
#
# Override the embedded version with: make VERSION=0.1.0

SHELL       := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all

GO      ?= go
PKG     := ./cmd/sdrnext
DIST    := dist
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)
LDFLAGS := -s -w -X main.version=$(VERSION)

HOST_OS   := $(shell $(GO) env GOOS)
HOST_ARCH := $(shell $(GO) env GOARCH)

# Reject versions that could break out of the -ldflags argument.
ifneq ($(shell printf '%s' '$(VERSION)' | grep -Eq '^[0-9A-Za-z._+-]{1,64}$$' && echo ok),ok)
$(error VERSION must match [0-9A-Za-z._+-]{1,64})
endif

# binary builds one GOOS/GOARCH pair: $(call binary,linux,arm64)
define binary
	@mkdir -p $(DIST)
	CGO_ENABLED=0 GOOS=$(1) GOARCH=$(2) $(GO) build -trimpath -ldflags '$(LDFLAGS)' \
		-o $(DIST)/sdrnext-$(1)-$(2)$(if $(filter windows,$(1)),.exe) $(PKG)
endef

.PHONY: all build linux cross test race vet fmt vuln check clean

all: check build

build:
	$(call binary,$(HOST_OS),$(HOST_ARCH))

linux:
	$(call binary,linux,amd64)
	$(call binary,linux,arm64)

cross: linux
	$(call binary,windows,amd64)
	$(call binary,windows,arm64)

test:
	CGO_ENABLED=0 $(GO) test ./...

race:
	CGO_ENABLED=1 $(GO) test -race ./...

vet:
	$(GO) vet ./...

fmt:
	@files="$$(gofmt -l .)"; \
	if [ -n "$$files" ]; then echo "not formatted:"; echo "$$files"; exit 1; fi

vuln:
	@command -v govulncheck >/dev/null || { \
		echo "govulncheck not found; install it with: go install golang.org/x/vuln/cmd/govulncheck@latest"; exit 1; }
	govulncheck ./...

check: fmt vet test

clean:
	rm -rf $(DIST)
