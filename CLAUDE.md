# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and flash

This is an Arduino sketch targeting the LOLIN D32 / D1 Mini32 (ESP32). There is no build system — compile and flash via the Arduino IDE or the `arduino-cli`.

```sh
# Compile
arduino-cli compile --fqbn esp32:esp32:d1_mini32 esp32-ticker-leds.ino

# Flash (adjust port as needed — macOS default is /dev/tty.usbserial-0001)
arduino-cli upload -p /dev/tty.usbserial-0001 --fqbn esp32:esp32:d1_mini32 esp32-ticker-leds.ino

# Monitor serial output (115200 baud)
arduino-cli monitor -p /dev/tty.usbserial-0001 --config baudrate=115200
```

The VS Code Arduino extension uses `.vscode/arduino.json` and `.vscode/c_cpp_properties.json` for IntelliSense; it targets the same board and port.

Required board package: **esp32 by Espressif Systems** (tested 3.3.7) — add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` as an additional boards manager URL.

Required libraries: **FastLED** 3.10.3, **ArduinoJson** v7.x (v6 API is incompatible).

## Secrets

`secrets.h` is gitignored. Copy `secrets.h.example` to `secrets.h` and fill in `WIFI_SSID`, `WIFI_PASSWORD`, and `FINNHUB_TOKEN`.

## Architecture

The project is two self-contained sketches:

**`esp32-ticker-leds.ino`** — the main sketch.
- `setup()`: initialises FastLED, shows a dim-white boot indicator, connects to WiFi (dim-orange on failure), then clears the LEDs.
- `loop()`: calls `pollAllTickers()` immediately on first run, then every `POLL_INTERVAL_MS` ms.
- `pollAllTickers()`: iterates `SYMBOLS[]`, calls `fetchQuote()` for each, sets `leds[i]` via `computeColor()`, then calls `FastLED.show()` once at the end.
- `fetchQuote()`: makes an HTTPS GET to `finnhub.io/api/v1/quote`, parses only the `dp` (percent change) field using ArduinoJson v7's filter API to minimise RAM usage. Certificate verification is skipped (`setInsecure()`).
- `computeColor()`: maps `|dp|` linearly into `[DIM_BRIGHTNESS, FULL_BRIGHTNESS]`, optionally passes it through `sigmoidMap()`, then returns green (up) or red (down). The `GRB` template argument to `FastLED.addLeds` handles wire byte-order; always use RGB values in code.

**`brightness_test/brightness_test.ino`** — standalone calibration sketch. Cycles through linear, sigmoid, and gamma ramps at six fixed brightness levels so you can visually compare curves on the physical strip. No WiFi or secrets needed.

## Key constraints

- **ArduinoJson v7 only.** `JsonDocument` (not `StaticJsonDocument`/`DynamicJsonDocument`). The filter API (`DeserializationOption::Filter`) is used to avoid allocating a full JSON doc on the heap.
- **Single `FastLED.show()` per poll cycle** — calling `show()` per LED causes visible flicker.
- **Free Finnhub tier** does not support index symbols (`^GSPC`, etc.) — use ETF proxies (SPY, QQQ, DIA).
