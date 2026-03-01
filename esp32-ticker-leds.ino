#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <math.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ---------------------------------------------------------------------------
// Hardware config
// ---------------------------------------------------------------------------
#define LED_PIN             23
#define NUM_LEDS             6
#define MARKET_OPEN_LED_PIN  2  // onboard "L" LED — HIGH when market is open

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define POLL_INTERVAL_MS (60UL * 1000UL)

// ---------------------------------------------------------------------------
// Brightness limits
// ---------------------------------------------------------------------------
#define DIM_BRIGHTNESS          20
#define FULL_BRIGHTNESS        255
#define USE_SIGMOID_BRIGHTNESS   1  // 0 = linear, 1 = sigmoid
#define CLOSED_MAX_BRIGHTNESS   20  // scale open-market brightness to this max when market is closed

// ---------------------------------------------------------------------------
// Price-change scale: ±MAX_PERCENT maps to full brightness
// ---------------------------------------------------------------------------
#define MAX_PERCENT 5.0f

// ---------------------------------------------------------------------------
// Ticker symbols (URL-encoded at compile time)
// ---------------------------------------------------------------------------
const char* const SYMBOLS[NUM_LEDS] = {
    "SPY",   // LED 0: S&P 500 (ETF proxy — ^GSPC requires paid Finnhub tier)
    "QQQ",   // LED 1: NASDAQ-100 (ETF proxy)
    "DIA",   // LED 2: DJIA (ETF proxy)
    "VCIT",  // LED 3: customize
    "NVDA",  // LED 4: customize
    "SO", // LED 5: customize
};

// ---------------------------------------------------------------------------
// LED array
// ---------------------------------------------------------------------------
CRGB leds[NUM_LEDS];

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void pollAllTickers();
bool fetchMarketStatus();
bool fetchQuote(const char* symbol, float* outDp);
CRGB computeColor(float dp, bool marketOpen);

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\nESP32 Stock Ticker LED Indicator — starting up");

    pinMode(MARKET_OPEN_LED_PIN, OUTPUT);
    digitalWrite(MARKET_OPEN_LED_PIN, LOW);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Boot indicator: all dim white
    fill_solid(leds, NUM_LEDS, CRGB(20, 20, 20));
    FastLED.show();

    // Connect to WiFi
    Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 30000UL) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection failed — check credentials");
        // All LEDs dim orange to signal WiFi failure
        fill_solid(leds, NUM_LEDS, CRGB(20, 10, 0));
        FastLED.show();
        return;
    }

    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());

    // Connected: clear LEDs
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    FastLED.show();
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
    static bool firstRun = true;
    static unsigned long lastPollTime = 0;

    // WiFi reconnection guard
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost — attempting reconnect, skipping poll");
        WiFi.reconnect();
        delay(5000);
        return;
    }

    if (firstRun || (millis() - lastPollTime >= POLL_INTERVAL_MS)) {
        firstRun = false;
        pollAllTickers();
        lastPollTime = millis();  // set AFTER poll completes
    }
}

// ---------------------------------------------------------------------------
// pollAllTickers — fetch all symbols, update LEDs once at the end
// ---------------------------------------------------------------------------
void pollAllTickers() {
    Serial.println("--- Polling tickers ---");
    bool marketOpen = fetchMarketStatus();
    digitalWrite(MARKET_OPEN_LED_PIN, marketOpen ? HIGH : LOW);

    for (int i = 0; i < NUM_LEDS; i++) {
        float dp = 0.0f;
        if (fetchQuote(SYMBOLS[i], &dp)) {
            leds[i] = computeColor(dp, marketOpen);
            Serial.printf("  [%d] %s  dp=%.2f%%  rgb=(%d,%d,%d)\n",
                          i, SYMBOLS[i], dp, leds[i].r, leds[i].g, leds[i].b);
        } else {
            leds[i] = CRGB(10, 10, 10);  // dim white = fetch failure
            Serial.printf("  [%d] %s  FETCH FAILED  rgb=(%d,%d,%d)\n",
                          i, SYMBOLS[i], leds[i].r, leds[i].g, leds[i].b);
        }
    }
    FastLED.show();  // single show after all LEDs set — no flicker
    Serial.println("--- Poll complete ---");
}

// ---------------------------------------------------------------------------
// fetchMarketStatus — returns true if US market is open; defaults to true on error
// ---------------------------------------------------------------------------
bool fetchMarketStatus() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url),
             "https://finnhub.io/api/v1/stock/market-status?exchange=US&token=%s",
             FINNHUB_TOKEN);

    if (!http.begin(client, url)) {
        Serial.println("  market-status: http.begin failed, assuming open");
        return true;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("  market-status: HTTP %d, assuming open\n", httpCode);
        http.end();
        return true;
    }

    JsonDocument filter;
    filter["isOpen"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    if (err || doc["isOpen"].isNull()) {
        Serial.println("  market-status: parse error, assuming open");
        return true;
    }

    bool isOpen = doc["isOpen"].as<bool>();
    Serial.printf("  market-status: isOpen=%s\n", isOpen ? "true" : "false");
    return isOpen;
}

// ---------------------------------------------------------------------------
// fetchQuote — returns true and writes dp on success
// ---------------------------------------------------------------------------
bool fetchQuote(const char* symbol, float* outDp) {
    WiFiClientSecure client;
    client.setInsecure();  // skip certificate verification

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url),
             "https://finnhub.io/api/v1/quote?symbol=%s&token=%s",
             symbol, FINNHUB_TOKEN);

    if (!http.begin(client, url)) {
        Serial.printf("    http.begin failed for %s\n", symbol);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("    HTTP error %d for %s\n", httpCode, symbol);
        http.end();
        return false;
    }

    // ArduinoJson v7: filter to save RAM
    JsonDocument filter;
    filter["dp"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("    JSON parse error for %s: %s\n", symbol, err.c_str());
        return false;
    }

    if (doc["dp"].isNull()) {
        Serial.printf("    Missing 'dp' field for %s\n", symbol);
        return false;
    }

    *outDp = doc["dp"].as<float>();
    return true;
}

// ---------------------------------------------------------------------------
// sigmoidMap — maps [0,255] through a sigmoid curve back to [0,255]
// ---------------------------------------------------------------------------
static uint8_t sigmoidMap(uint8_t v) {
    float x = (v / 255.0f) * 10.0f - 5.0f;
    float s = 1.0f / (1.0f + expf(-x));
    const float lo = 1.0f / (1.0f + expf(5.0f));
    const float hi = 1.0f / (1.0f + expf(-5.0f));
    return (uint8_t)((s - lo) / (hi - lo) * 255.0f);
}

// ---------------------------------------------------------------------------
// computeColor — brightness proportional to |dp|, green=up red=down
// ---------------------------------------------------------------------------
CRGB computeColor(float dp, bool marketOpen) {
    float absDP = fabsf(dp);
    if (absDP > MAX_PERCENT) absDP = MAX_PERCENT;

    float t = absDP / MAX_PERCENT;  // 0.0 … 1.0
    uint8_t brightness = (uint8_t)(DIM_BRIGHTNESS + t * (FULL_BRIGHTNESS - DIM_BRIGHTNESS));
#if USE_SIGMOID_BRIGHTNESS
    brightness = sigmoidMap(brightness);
#endif

    if (!marketOpen) {
        // Scale the open-market brightness proportionally into [1, CLOSED_MAX_BRIGHTNESS],
        // preserving relative differences between small and large moves but keeping
        // all LEDs dim. Floor of 1 ensures the LED is never fully off.
        brightness = max((uint8_t)1, (uint8_t)(brightness * CLOSED_MAX_BRIGHTNESS / 255.0f));
    }

    if (dp >= 0.0f) {
        return CRGB(0, brightness, 0);  // green — up
    } else {
        return CRGB(brightness, 0, 0);  // red — down
    }
    // Note: CRGB(r,g,b) is always RGB order in code;
    // the GRB template arg in addLeds handles wire reordering.
}
