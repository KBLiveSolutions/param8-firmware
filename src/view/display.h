#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "faderWidget.h"

#define OVERLAY_TIME 200 // Temps d'affichage de la boîte de dialogue en millisecondes
extern U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2;
extern U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2_2;
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

void setupDisplay() ;

void showDisplay();
void updateFader(int idx, int value);
void updateDisplay();
void updateDisplayBox(const char* side, const char* text, bool isStatic = false);
void drawDeviceBankLabels();
void runScreenSaver();