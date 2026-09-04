#include "leds.h"

static const uint8_t ledPins[] = { LED_PIN_1, LED_PIN_2 };

int ledBrightness = 255;
static const uint8_t LED_DIM = 64;     // 25%
static const uint8_t LED_FULL = 255;   // 100%
static bool ledBlinking[2] = {false, false};
static unsigned long lastBlinkTime[2] = {0, 0};
static bool blinkState[2] = {false, false};

void setupLeds() {
  for (auto pin : ledPins) {
    pinMode(pin, OUTPUT);
    analogWrite(pin, LED_DIM);
  }
  Serial.println("LEDs initialized (GPIO 0, 29)");
}

void setLed(int led, bool on) {
  if (led < 2) {
    ledBlinking[led] = false;
    analogWrite(ledPins[led], on ? LED_FULL : LED_DIM);
  }
}

void setLedBlink(int led, bool blink) {
  if (led >= 2) return;
  ledBlinking[led] = blink;
  if (blink) {
    blinkState[led] = true;
    lastBlinkTime[led] = millis();
    analogWrite(ledPins[led], LED_FULL);
  } else {
    analogWrite(ledPins[led], LED_DIM);
  }
}

void updateLeds() {
  unsigned long now = millis();
  for (int i = 0; i < 2; i++) {
    if (!ledBlinking[i]) continue;
    if (now - lastBlinkTime[i] >= 300) {
      blinkState[i] = !blinkState[i];
      lastBlinkTime[i] = now;
      analogWrite(ledPins[i], blinkState[i] ? LED_FULL : 0);
    }
  }
}

void setLedBrightness(int led, uint8_t value) {
  if (led < 2)
    analogWrite(ledPins[led], value);
}
