#include "leds.h"
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int ledBrightness = 10;

void setupLeds(){
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(ledBrightness); // Set brightness
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0)); // Initialize all LEDs to off
  }
  pixels.show(); // Update the strip to apply the initial state
  Serial.println("LEDs initialized");
  // showLeds(); // Uncomment if you want to show all LEDs initially
}

void showLed(int led, uint8_t r, uint8_t g, uint8_t b) {
  if (led < NUMPIXELS) {
    Serial.printf("showLed: %d, r: %d, g: %d, b: %d\n", led, r, g, b);
    pixels.setPixelColor(led, pixels.Color(r, g, b));
    pixels.show();
  }
}

void blinkLedBlue(int led, int times) {
  if (led < NUMPIXELS) {
    for (int i = 0; i < times; i++) {
      // Allumer la LED en bleu
      pixels.setPixelColor(led, pixels.Color(0, 0, 255));
      pixels.show();
      delay(100); // Garder allumée pendant 200ms
      
      // Éteindre la LED
      pixels.setPixelColor(led, pixels.Color(0, 0, 0));
      pixels.show();
      delay(100); // Garder éteinte pendant 200ms
    }
  }
}