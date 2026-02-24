#include <FastLED.h>
#include <math.h>

#define LED_PIN   23
#define NUM_LEDS   6

CRGB leds[NUM_LEDS];

const uint8_t LEVELS[NUM_LEDS] = { 5, 55, 105, 155, 205, 255 };

uint8_t linearMap(uint8_t v) {
    return v;
}

uint8_t sigmoidMap(uint8_t v) {
    float x = (v / 255.0f) * 10.0f - 5.0f;
    float s = 1.0f / (1.0f + expf(-x));
    const float lo = 1.0f / (1.0f + expf(5.0f));
    const float hi = 1.0f / (1.0f + expf(-5.0f));
    return (uint8_t)((s - lo) / (hi - lo) * 255.0f);
}

uint8_t gammaMap(uint8_t v) {
    return (uint8_t)(powf(v / 255.0f, 1.0f / 2.2f) * 255.0f);
}

void showColor(uint8_t r, uint8_t g, uint8_t b, uint8_t (*mapFn)(uint8_t)) {
    for (int i = 0; i < NUM_LEDS; i++) {
        float scale = mapFn(LEVELS[i]) / 255.0f;
        leds[i] = CRGB((uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
    }
    FastLED.show();
}

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
}

void loop() {
    showColor(0, 255, 0, linearMap);    // green linear
    delay(5000);
    showColor(255, 0, 0, linearMap);    // red linear
    delay(5000);
    showColor(0, 255, 0, sigmoidMap);   // green sigmoid
    delay(5000);
    showColor(255, 0, 0, sigmoidMap);   // red sigmoid
    delay(5000);
    showColor(0, 255, 0, gammaMap);     // green gamma
    delay(5000);
    showColor(255, 0, 0, gammaMap);     // red gamma
    delay(5000);
}
