#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define PIN        0 
#define NUMPIXELS 8 

// extern Adafruit_NeoPixel pixels;
extern int ledBrightness;
void setupLeds();
void showLed(int led, uint8_t r, uint8_t g, uint8_t b);
void blinkLedBlue(int led, int times);