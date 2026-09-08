#pragma once
#include <Arduino.h>
#include <PicoGFX_SSD1322.h>
#include "faderWidget.h"

extern PicoGFX_SSD1322 display1;
extern PicoGFX_SSD1322 display2;
extern FaderWidget* faders[8];

extern bool screenSaverActive;
extern unsigned long screenSaverDelay;
extern char deviceLabel[20];
extern char bankLabel[20];
extern bool deviceLabelDirty;
extern bool bankLabelDirty;

// Helper: pixel width of str using the current font on disp
// TODO: result accuracy depends on current font; call after setFont()
inline int getStrWidth(PicoGFX_SSD1322 &disp, const char* str) {
    int16_t x1, y1;
    uint16_t w, h;
    disp.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    return (int)w;
}

void setupDisplay();

void showDisplay();
void flushDisplays();
void updateFader(int idx, int value);
void updateDisplay();
void drawDeviceBankLabels();
void runScreenSaver();
void setBrightness(uint8_t value);
