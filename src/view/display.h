#pragma once
#include <Arduino.h>
#include <PicoGFX_SSD1322.h>
#include "faderWidget.h"

#define OVERLAY_TIME 200 // Temps d'affichage de la boîte de dialogue en millisecondes
extern PicoGFX_SSD1322 display1;
extern PicoGFX_SSD1322 display2;
extern FaderWidget* faders[8];

extern char left_box_text[20];
extern char right_box_text[20];
extern unsigned long display_start_time;
extern bool display_active;
extern bool display_needs_update;
extern bool staticOverlay;
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
void updateFader(int idx, int value);
void updateDisplay();
void updateDisplayBox(const char* side, const char* text, bool isStatic = false);
void drawDeviceBankLabels();
void runScreenSaver();
void setBrightness(uint8_t value);
