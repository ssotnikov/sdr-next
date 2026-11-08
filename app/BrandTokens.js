.pragma library
// SDR Next brand tokens, adapted from the supplied theme JSON.
// Qt uses #AARRGGBB; the original theme uses #RRGGBBAA.

var themes = {
  "dark": {
    "$schema": "sdr-next-theme.schema.json",
    "format": 1,
    "id": "sdr-next-dark",
    "name": "SDR Next — тёмная",
    "base": "dark",
    "colors": {
      "bg0": "#ff0a0d10",
      "bg1": "#ff10151a",
      "bg2": "#ff172029",
      "bg3": "#ff202c37",
      "line": "#ff26313c",
      "lineControl": "#ff5d6b79",
      "ink": "#ffe6ebf0",
      "inkMuted": "#ffa1adb9",
      "inkFaint": "#ff808d99",
      "dial": "#ffffb224",
      "dialHover": "#ffffc457",
      "onDial": "#ff1a1204",
      "dialInk": "#ffffb224",
      "dialSoft": "#ff2e2208",
      "onVfo": "#ff1a1204",
      "trace": "#ff8fd8ff",
      "traceFill": "#338fd8ff",
      "focus": "#ff8fd8ff",
      "ok": "#ff5cb8ff",
      "warn": "#ffff9a52",
      "danger": "#ffff6b78",
      "rec": "#ffff4d5e",
      "okSoft": "#ff0f2638",
      "warnSoft": "#ff33200f",
      "dangerSoft": "#ff3a1419"
    },
    "vfo": [
      "#ffffb224",
      "#ff5cc8ff",
      "#ff7edc8a",
      "#ffff85c0",
      "#ffb9a3ff",
      "#fff2e35c",
      "#ff52e0c9",
      "#ffff9a6b"
    ],
    "waterfall": {
      "id": "next",
      "name": "Next",
      "stops": [
        {
          "pos": 0,
          "color": "#ff0a0d10"
        },
        {
          "pos": 0.2,
          "color": "#ff0c2a4c"
        },
        {
          "pos": 0.4,
          "color": "#ff0f6b8e"
        },
        {
          "pos": 0.6,
          "color": "#ff3cc0a3"
        },
        {
          "pos": 0.8,
          "color": "#ffffd23f"
        },
        {
          "pos": 0.92,
          "color": "#ffff7a1a"
        },
        {
          "pos": 1,
          "color": "#fffff4e0"
        }
      ]
    },
    "fonts": {
      "sans": {
        "family": "IBM Plex Sans",
        "files": {
          "400": "IBMPlexSans-Regular.ttf",
          "500": "IBMPlexSans-Medium.ttf",
          "600": "IBMPlexSans-SemiBold.ttf"
        }
      },
      "mono": {
        "family": "IBM Plex Mono",
        "files": {
          "400": "IBMPlexMono-Regular.ttf",
          "500": "IBMPlexMono-Medium.ttf"
        }
      }
    },
    "text": {
      "display": {
        "family": "sans",
        "size": 40.0,
        "lineHeight": 44.0,
        "weight": 600,
        "tracking": -0.02
      },
      "title": {
        "family": "sans",
        "size": 20.0,
        "lineHeight": 28.0,
        "weight": 600,
        "tracking": -0.01
      },
      "heading": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 600
      },
      "body": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 400
      },
      "body-strong": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 500
      },
      "caption": {
        "family": "sans",
        "size": 12.0,
        "lineHeight": 16.0,
        "weight": 400
      },
      "label": {
        "family": "sans",
        "size": 11.0,
        "lineHeight": 16.0,
        "weight": 600,
        "tracking": 0.06,
        "uppercase": true
      },
      "freq-xl": {
        "family": "mono",
        "size": 32.0,
        "lineHeight": 36.0,
        "weight": 500
      },
      "freq": {
        "family": "mono",
        "size": 16.0,
        "lineHeight": 20.0,
        "weight": 500
      },
      "readout": {
        "family": "mono",
        "size": 11.0,
        "lineHeight": 16.0,
        "weight": 400
      },
      "code": {
        "family": "mono",
        "size": 12.0,
        "lineHeight": 18.0,
        "weight": 400
      }
    },
    "metrics": {
      "spacing": {
        "space-0": 2.0,
        "space-1": 4.0,
        "space-2": 8.0,
        "space-3": 12.0,
        "space-4": 16.0,
        "space-6": 24.0,
        "space-8": 32.0
      },
      "radius": {
        "radius-0": 0.0,
        "radius-sm": 3.0,
        "radius-md": 6.0,
        "radius-lg": 10.0,
        "radius-full": 999
      },
      "size": {
        "control-sm": 22.0,
        "control-md": 28.0,
        "control-lg": 36.0,
        "sidebar-w": 288.0
      }
    },
    "opacity": {
      "vfo-band": 0.18,
      "vfo-band-active": 0.28,
      "disabled": 0.4
    },
    "focusRing": {
      "gap": 2,
      "width": 2,
      "color": "focus"
    },
    "floatShadow": {
      "color": "#b3000000",
      "offsetY": 8,
      "blur": 24
    }
  },
  "light": {
    "$schema": "sdr-next-theme.schema.json",
    "format": 1,
    "id": "sdr-next-light",
    "name": "SDR Next — светлая",
    "base": "light",
    "colors": {
      "bg0": "#fff4f6f8",
      "bg1": "#ffffffff",
      "bg2": "#ffeef1f4",
      "bg3": "#ffe2e7ec",
      "line": "#ffd6dde4",
      "lineControl": "#ff7f8c99",
      "ink": "#ff0e141a",
      "inkMuted": "#ff4a5562",
      "inkFaint": "#ff606b77",
      "dial": "#ffffb224",
      "dialHover": "#fff0a010",
      "onDial": "#ff1a1204",
      "dialInk": "#ff8a4b00",
      "dialSoft": "#fffff0cf",
      "onVfo": "#ffffffff",
      "trace": "#ff0b5f8a",
      "traceFill": "#260b5f8a",
      "focus": "#ff0b5f8a",
      "ok": "#ff0a5fae",
      "warn": "#ffa44a00",
      "danger": "#ffc01b2e",
      "rec": "#ffd0182c",
      "okSoft": "#ffe3f1ff",
      "warnSoft": "#fffff0e2",
      "dangerSoft": "#ffffe8ea"
    },
    "vfo": [
      "#ff8a4b00",
      "#ff0b5f8a",
      "#ff1f6e2c",
      "#ffa3195f",
      "#ff5b3fc4",
      "#ff6b6100",
      "#ff0b6b5e",
      "#ffa33e10"
    ],
    "waterfall": {
      "id": "next",
      "name": "Next",
      "stops": [
        {
          "pos": 0,
          "color": "#ff0a0d10"
        },
        {
          "pos": 0.2,
          "color": "#ff0c2a4c"
        },
        {
          "pos": 0.4,
          "color": "#ff0f6b8e"
        },
        {
          "pos": 0.6,
          "color": "#ff3cc0a3"
        },
        {
          "pos": 0.8,
          "color": "#ffffd23f"
        },
        {
          "pos": 0.92,
          "color": "#ffff7a1a"
        },
        {
          "pos": 1,
          "color": "#fffff4e0"
        }
      ]
    },
    "fonts": {
      "sans": {
        "family": "IBM Plex Sans",
        "files": {
          "400": "IBMPlexSans-Regular.ttf",
          "500": "IBMPlexSans-Medium.ttf",
          "600": "IBMPlexSans-SemiBold.ttf"
        }
      },
      "mono": {
        "family": "IBM Plex Mono",
        "files": {
          "400": "IBMPlexMono-Regular.ttf",
          "500": "IBMPlexMono-Medium.ttf"
        }
      }
    },
    "text": {
      "display": {
        "family": "sans",
        "size": 40.0,
        "lineHeight": 44.0,
        "weight": 600,
        "tracking": -0.02
      },
      "title": {
        "family": "sans",
        "size": 20.0,
        "lineHeight": 28.0,
        "weight": 600,
        "tracking": -0.01
      },
      "heading": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 600
      },
      "body": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 400
      },
      "body-strong": {
        "family": "sans",
        "size": 13.0,
        "lineHeight": 20.0,
        "weight": 500
      },
      "caption": {
        "family": "sans",
        "size": 12.0,
        "lineHeight": 16.0,
        "weight": 400
      },
      "label": {
        "family": "sans",
        "size": 11.0,
        "lineHeight": 16.0,
        "weight": 600,
        "tracking": 0.06,
        "uppercase": true
      },
      "freq-xl": {
        "family": "mono",
        "size": 32.0,
        "lineHeight": 36.0,
        "weight": 500
      },
      "freq": {
        "family": "mono",
        "size": 16.0,
        "lineHeight": 20.0,
        "weight": 500
      },
      "readout": {
        "family": "mono",
        "size": 11.0,
        "lineHeight": 16.0,
        "weight": 400
      },
      "code": {
        "family": "mono",
        "size": 12.0,
        "lineHeight": 18.0,
        "weight": 400
      }
    },
    "metrics": {
      "spacing": {
        "space-0": 2.0,
        "space-1": 4.0,
        "space-2": 8.0,
        "space-3": 12.0,
        "space-4": 16.0,
        "space-6": 24.0,
        "space-8": 32.0
      },
      "radius": {
        "radius-0": 0.0,
        "radius-sm": 3.0,
        "radius-md": 6.0,
        "radius-lg": 10.0,
        "radius-full": 999
      },
      "size": {
        "control-sm": 22.0,
        "control-md": 28.0,
        "control-lg": 36.0,
        "sidebar-w": 288.0
      }
    },
    "opacity": {
      "vfo-band": 0.18,
      "vfo-band-active": 0.28,
      "disabled": 0.4
    },
    "focusRing": {
      "gap": 2,
      "width": 2,
      "color": "focus"
    },
    "floatShadow": {
      "color": "#290e141a",
      "offsetY": 8,
      "blur": 24
    }
  }
};
