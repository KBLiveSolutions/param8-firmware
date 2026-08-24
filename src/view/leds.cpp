#include "leds.h"

static const uint8_t ledPins[] = { LED_PIN_1, LED_PIN_2 };

int ledBrightness = 255;
static const uint8_t LED_DIM = 64;     // 25%
static const uint8_t LED_FULL = 255;   // 100%

void setupLeds() {
  for (auto pin : ledPins) {
    pinMode(pin, OUTPUT);
    analogWrite(pin, LED_DIM);
  }
  Serial.println("LEDs initialized (GPIO 0, 29)");
}

void setLed(int led, bool on) {
  if (led < 2)
    analogWrite(ledPins[led], on ? LED_FULL : LED_DIM);
}

void setLedBrightness(int led, uint8_t value) {
  if (led < 2)
    analogWrite(ledPins[led], value);
}
