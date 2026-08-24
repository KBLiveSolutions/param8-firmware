#pragma once
#include <Arduino.h>

#define LED_PIN_1  0
#define LED_PIN_2  29

extern int ledBrightness;
void setupLeds();
void setLed(int led, bool on);
void setLedBrightness(int led, uint8_t value);
