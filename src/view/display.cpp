#include <Arduino.h>
#include "display.h"
#include "../core/actions.h"
#include "../core/controls.h"
#include "../midi/midi.h"
#include "fonts/MiscFixed7x13Bpt7b.h"


FaderWidget* faders[8];


char deviceLabel[20] = "Device";
char bankLabel[20] = "Bank";
char trackLabel[20] = "Track";
char presetLabel[20] = {0};
char presetNumberLabel[20] = {0};
bool deviceLabelDirty = false;
bool bankLabelDirty = false;
bool trackLabelDirty = false;
bool presetLabelDirty = false;
uint8_t currentLabelConfig[4] = {1, 4, 2, 3};
bool screenSaverActive = false;
bool screenSaverJustWoke = false;
uint8_t brightnessLevel = 1;

void setupDisplay() {
    display1.begin();
    display1.displayOff(); // begin() ends with DISPLAYON but GRAM is still random
    display2.begin();
    display2.displayOff();

    // Fix remap for ZJY panels: enable nibble remap to un-mirror
    // Bits: A[0]=col remap, A[1]=nibble remap, A[2]=addr inc, A[4]=COM remap
    // Default lib value is 0x14. ZJY panels need nibble remap (bit 1).
    // Try 0x16 first; if still mirrored, try 0x17 (add col remap too).
    display1.oled_command(SSD1322_SEGREMAP);
    display1.oled_data(0x16);   // nibble remap=1, COM remap=1
    display1.oled_data(0x11);   // dual COM mode
    display2.oled_command(SSD1322_SEGREMAP);
    display2.oled_data(0x16);
    display2.oled_data(0x11);

    // Adjust column offset if display is shifted (16px = 4 columns)
    // Standard is 28; try 24 if shifted right, 32 if shifted left
    display1.col_offset = 24;
    display2.col_offset = 24;

    // Display timing — anti-ghosting settings
    display1.oled_command(SSD1322_PHASELEN);  display1.oled_data(0xF8); // phase2=15, phase1=8 DCLKs (long discharge)
    display1.oled_command(SSD1322_PRECHARGE); display1.oled_data(0x17);
    display1.oled_command(SSD1322_SETVCOM);   display1.oled_data(0x01); // very low VCOMH = less deselect leakage
    // Internal VSL + second precharge (key for ghosting on ZJY panels)
    display1.oled_command(0xB4); display1.oled_data(0xA2); display1.oled_data(0xFD);
    display1.oled_command(0xB6); display1.oled_data(0x0F); // max second precharge period
    display1.oled_command(0xB9);                            // linear gray scale — level 0 = truly dark
    display2.oled_command(SSD1322_PHASELEN);  display2.oled_data(0xF8);
    display2.oled_command(SSD1322_PRECHARGE); display2.oled_data(0x17);
    display2.oled_command(SSD1322_SETVCOM);   display2.oled_data(0x01);
    display2.oled_command(0xB4); display2.oled_data(0xA2); display2.oled_data(0xFD);
    display2.oled_command(0xB6); display2.oled_data(0x0F);
    display2.oled_command(0xB9);

    // display1: faders 1,2,5,6
    faders[0] = new FaderWidget(display1, "Fader 1",   0,   0, 0);
    faders[1] = new FaderWidget(display1, "Fader 2", 128,   0, 1);
    faders[4] = new FaderWidget(display1, "Fader 5",   0,  32, 4);
    faders[5] = new FaderWidget(display1, "Fader 6", 128,  32, 5);

    // display2: faders 3,4,7,8
    faders[2] = new FaderWidget(display2, "Fader 3",   0,   0, 2);
    faders[3] = new FaderWidget(display2, "Fader 4", 128,   0, 3);
    faders[6] = new FaderWidget(display2, "Fader 7",   0,  32, 6);
    faders[7] = new FaderWidget(display2, "Fader 8", 128,  32, 7);

    // Push black to GRAM, wait for DMA, then turn ON — panel stays dark until clean.
    display1.fillScreen(0);
    display1.display();
    while (!display1.isTransferComplete()) {}
    display1.displayOn();

    display2.fillScreen(0);
    display2.display();
    while (!display2.isTransferComplete()) {}
    display2.displayOn();

    setBrightness(brightnessLevel);
}

static const uint8_t LABEL_BACKGROUND = 5;

static void drawLabelOverButton(PicoGFX_SSD1322 &disp, const char* txt, int y) {
    int label_w = 64;
    int label_x = (256 - label_w) / 2;
    int inner_w = label_w - 4;
    int area_h = 10;

    disp.fillRect(label_x, y, label_w, area_h, 0);
    disp.fillRect(label_x + 2, y, inner_w, area_h, LABEL_BACKGROUND);

    disp.setFont(NULL);
    int tw = getStrWidth(disp, txt);

    if (tw <= inner_w) {
        int text_x = label_x + (label_w - tw) / 2;
        disp.setCursor(text_x, y + 1);
        disp.setTextColor(0);
        disp.print(txt);
    } else {
        char truncated[20];
        strncpy(truncated, txt, sizeof(truncated));
        truncated[sizeof(truncated)-1] = '\0';
        while (strlen(truncated) > 1 && getStrWidth(disp, truncated) > inner_w - 4) {
            truncated[strlen(truncated)-1] = '\0';
        }
        disp.setCursor(label_x + 5, y + 1);
        disp.setTextColor(0);
        disp.print(truncated);
    }
}

// Label type constants: 0=Off, 1=Track, 2=Device, 3=Bank, 4=Preset Name, 5=Preset Number
static const char* getLabelByType(uint8_t type) {
    switch (type) {
        case 1: return trackLabel;
        case 2: return deviceLabel;
        case 3: return bankLabel;
        case 4: return presetLabel;
        case 5: return presetNumberLabel;
        default: return nullptr;
    }
}

// Slot positions: 0=display1 top, 1=display2 top, 2=display1 bottom, 3=display2 bottom
static void drawOrClearSlot(uint8_t slot) {
    static const struct { PicoGFX_SSD1322* disp; int y; } positions[4] = {
        { &display1, 0 }, { &display2, 0 }, { &display1, 54 }, { &display2, 54 }
    };
    uint8_t type = currentLabelConfig[slot];
    const char* lbl = (type == 0) ? nullptr : getLabelByType(type);
    if (lbl && lbl[0] != '\0') {
        drawLabelOverButton(*positions[slot].disp, lbl, positions[slot].y);
    } else {
        // clear the slot area
        positions[slot].disp->fillRect((256 - 64) / 2, positions[slot].y, 64, 10, 0);
    }
}

static bool isFlushing = false;
static bool pendingShow = false;

void flushDisplays() {
    if (isFlushing) return;
    isFlushing = true;
    if (display1.isDirty()) {
        display1.display();
        while (!display1.isTransferComplete()) {
            midiRead();
        }
    }
    if (display2.isDirty()) {
        display2.display();
        while (!display2.isTransferComplete()) {
            midiRead();
        }
    }
    isFlushing = false;
    if (pendingShow) {
        pendingShow = false;
        showDisplay();
    }
}

void drawAllWidgets() {
    display1.fillScreen(0);
    display2.fillScreen(0);

    for (int i : {0, 1, 4, 5}) {
        faders[i]->draw();
    }
    for (int i : {2, 3, 6, 7}) {
        faders[i]->draw();
    }
    uint8_t currentPreset = controls.getPreset();
    const char* pName = controls.getPresetName(currentPreset);
    if (pName && pName[0] != '\0') {
        strncpy(presetLabel, pName, sizeof(presetLabel) - 1);
    } else if (currentPreset == 24) {
        strncpy(presetLabel, "Global", sizeof(presetLabel) - 1);
    } else if (currentPreset == 25) {
        strncpy(presetLabel, "Device", sizeof(presetLabel) - 1);
    } else {
        snprintf(presetLabel, sizeof(presetLabel), "Preset %d", currentPreset + 1);
    }
    presetLabel[sizeof(presetLabel) - 1] = '\0';
    if (currentPreset < 24)
        snprintf(presetNumberLabel, sizeof(presetNumberLabel), "Preset %d", currentPreset + 1);
    else
        presetNumberLabel[0] = '\0';

    for (int i = 0; i < 4; i++) drawOrClearSlot(i);
    deviceLabelDirty = false;
    bankLabelDirty = false;
    trackLabelDirty = false;
    presetLabelDirty = false;
}

void showDisplay() {
    if (isFlushing) { pendingShow = true; return; }
    pendingShow = false;
    drawAllWidgets();
    flushDisplays();
}

void updateFader(int idx, int value) {
    faders[idx]->setValue(value);
}

void updateDisplay() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    if (now - lastUpdate < 25) return;
    lastUpdate = now;
    if (screenSaverActive) return;
    if (screenSaverJustWoke) {
        screenSaverJustWoke = false;
        showDisplay();
        return;
    }
    if (settingsModeActive || isSelectingPreset()) {
        for (int i = 0; i < 8; i++) faders[i]->_dirty = false;
        trackLabelDirty = bankLabelDirty = deviceLabelDirty = presetLabelDirty = false;
        return;
    }
    for (int i = 0; i < 8; i++) {
        if (faders[i]->_dirty) {
            faders[i]->drawFader();
            faders[i]->_dirty = false;
        }
    }
    drawDeviceBankLabels();
    flushDisplays();
}

void drawDeviceBankLabels() {
    if (screenSaverActive) return;
    if (!deviceLabelDirty && !bankLabelDirty && !trackLabelDirty && !presetLabelDirty) return;
    for (int i = 0; i < 4; i++) drawOrClearSlot(i);
    deviceLabelDirty = false;
    bankLabelDirty = false;
    trackLabelDirty = false;
    presetLabelDirty = false;
}

void runScreenSaver() {
    static unsigned long lastUpdate = 0;
    static float x1 = 30.0f,  y1 = 10.0f, dx1 =  1.3f, dy1 = 0.7f;
    static float x2 = 140.0f, y2 = 40.0f, dx2 = -1.1f, dy2 = 0.9f;
    static int textW = 0, textH = 0, cursorYOff = 0;
    const unsigned long frameInterval = 50;

    if (textW == 0) {
        display1.setFont(&MiscFixed7x13Bpt7b);
        int16_t bx, by; uint16_t bw, bh;
        display1.getTextBounds("param8", 0, 0, &bx, &by, &bw, &bh);
        textW = (int)bw;
        textH = (int)bh;
        cursorYOff = -by;   // cursor_y = iy + cursorYOff → text top aligns with iy
        display1.setFont(NULL);
    }

    unsigned long now = millis();
    if (now - lastUpdate < frameInterval) return;
    lastUpdate = now;

    int maxX = 256 - textW, maxY = 64 - textH;

    auto bounce = [](float &p, float &v, int hi) {
        p += v;
        if (p <= 0.0f)       { p = 0.0f;       v = -v; }
        if (p >= (float)hi)  { p = (float)hi;   v = -v; }
    };
    bounce(x1, dx1, maxX);  bounce(y1, dy1, maxY);
    bounce(x2, dx2, maxX);  bounce(y2, dy2, maxY);

    display1.fillScreen(0);
    display1.setFont(&MiscFixed7x13Bpt7b);
    display1.setTextColor(15);
    display1.setCursor((int)x1, (int)y1 + cursorYOff);
    display1.print("param8");

    display2.fillScreen(0);
    display2.setFont(&MiscFixed7x13Bpt7b);
    display2.setTextColor(15);
    display2.setCursor((int)x2, (int)y2 + cursorYOff);
    display2.print("param8");

    display1.displayBlocking();
    display2.displayBlocking();
}

void setBrightness(uint8_t level) {
    static const uint8_t contrasts[] = { 0x40, 0x80, 0xD0 };
    if (level > 2) level = 2;
    brightnessLevel = level;
    display1.setContrast(contrasts[level]);
    display2.setContrast(contrasts[level]);
    // TODO: SSD1322 master current (cmd 0xC7) has no direct PicoGFX_SSD1322 API.
    // Previously: sendF("ca", 0xC7, currents[level]) where currents = { 0x06, 0x0A, 0x0F }.
}
