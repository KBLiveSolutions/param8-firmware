#include "leds.h"
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int ledBrightness = 10;

void setupLeds(){
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(ledBrightness); // Set brightness
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(30, 20, 60)); // Initialize all LEDs to off
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

void pulseLedWhite(int led) {
  if (led < NUMPIXELS) {
    // Pulse continu en blanc - cette fonction devra être appelée régulièrement
    static unsigned long lastUpdate = 0;
    static int brightness = 0;
    static int direction = 1; // 1 pour augmenter, -1 pour diminuer
    
    if (millis() - lastUpdate > 20) { // Mise à jour toutes les 20ms
      brightness += direction * 5;
      
      if (brightness >= 255) {
        brightness = 255;
        direction = -1;
      } else if (brightness <= 0) {
        brightness = 0;
        direction = 1;
      }
      
      pixels.setPixelColor(led, pixels.Color(brightness, brightness, brightness));
      pixels.show();
      lastUpdate = millis();
    }
  }
}