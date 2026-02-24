# esp32-ticker-leds

Six WS2812B LEDs show live stock price movement: green for up, red for down, with brightness proportional to the magnitude of the change. The ESP32 polls [Finnhub](https://finnhub.io) every 60 seconds and updates all LEDs at once.

---

## Hardware

**Parts**
- LOLIN D32 / D1 Mini32 (ESP32)
- WS2812B LED strip, 6 LEDs

**Wiring**

| LED strip | Connects to |
|-----------|-------------|
| VCC (5V)  | External 5V PSU + |
| GND       | External 5V PSU − and board GND |
| DIN       | GPIO 23 |

Notes:
- The strip has a built-in series resistor on the data line; no external components are needed.
- Share ground between the PSU and the board.
- ESP32 3.3V data works with 5V-powered WS2812B strips in practice, especially with a series resistor.

---

## Software setup

### Board package (install once)

1. Open **Board Manager** in your IDE.
2. Add this URL to the additional boards manager URLs setting:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install **esp32 by Espressif Systems** (tested with 3.3.7).
4. Select board: **LOLIN D32** (or D1 Mini32).

### Libraries (install via Library Manager)

| Library | Author | Version |
|---------|--------|---------|
| FastLED | Daniel Garcia | tested 3.10.3 |
| ArduinoJson | Benoit Blanchon | **v7.x required** — v6 API is incompatible |

---

## Secrets

Copy `secrets.h.example` to `secrets.h` and fill in the three values:

```sh
cp secrets.h.example secrets.h
```

| Define | What to put |
|--------|-------------|
| `WIFI_SSID` | Your WiFi network name |
| `WIFI_PASSWORD` | Your WiFi password |
| `FINNHUB_TOKEN` | Finnhub API key — free tier at finnhub.io |

`secrets.h` is gitignored and is never committed.

**Finnhub free tier:** Covers US equities and ETFs. Index symbols like `^GSPC` require a paid plan — use ETF proxies instead (SPY, QQQ, DIA).

---

## Configuration

All user-configurable constants are at the top of `esp32-ticker-leds.ino`:

| Constant | Default | Description |
|----------|---------|-------------|
| `LED_PIN` | `23` | GPIO pin connected to strip DIN |
| `NUM_LEDS` | `6` | Number of LEDs in the strip |
| `POLL_INTERVAL_MS` | `60000` | Milliseconds between Finnhub polls |
| `DIM_BRIGHTNESS` | `20` | Brightness at 0% price change (floor) — keeps LEDs visible on flat days |
| `FULL_BRIGHTNESS` | `255` | Brightness at ±`MAX_PERCENT` change (ceiling) |
| `USE_SIGMOID_BRIGHTNESS` | `1` | `1` = sigmoid curve, `0` = linear brightness mapping |
| `MAX_PERCENT` | `3.0` | Price change (%) that saturates to full brightness |
| `SYMBOLS[]` | `SPY, QQQ, DIA, VCIT, NVDA, SO` | Ticker per LED — index 0 is the first LED |

**`MAX_PERCENT`:** A move of this size or larger lights the LED at full brightness. Tighten for quieter markets, loosen for volatile ones.

**`USE_SIGMOID_BRIGHTNESS`:** Sigmoid gives more perceptual differentiation in the mid-range; linear is simpler to reason about.

---

## Status indicators

| LED state at boot | Meaning |
|-------------------|---------|
| All dim white | Connecting to WiFi |
| All dim orange | WiFi failed — check credentials |
| All off | Connected, waiting for first poll |
| Dim white (single LED) | That symbol failed to fetch |

---

## brightness_test sketch (optional)

`brightness_test/` is a utility sketch that cycles through linear, sigmoid, and gamma brightness ramps to visually calibrate the strip. Flash it standalone if you want to re-evaluate the brightness curve. It is not needed for normal use.
