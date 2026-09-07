#include <Arduino.h>
#include "display.h"
#include "../core/actions.h"
#include "../core/controls.h"


FaderWidget* faders[8];


char left_box_text[20] = {"param8"};
char right_box_text[20] = {"KBD"};
char deviceLabel[20] = {0};
char bankLabel[20] = {0};
bool deviceLabelDirty = false;
bool bankLabelDirty = false;
unsigned long display_start_time = 0;
bool display_active = false;
bool display_needs_update = false;
bool staticOverlay = false;
bool screenSaverActive = false;

void setupDisplay() {
    display1.begin();
    display2.begin();

    // TODO: SSD1322 display timing registers (phase/pre-charge/VCOMH) have no
    // direct PicoGFX_SSD1322 API. Previously set via U8g2 sendF:
    //   sendF("ca", 0xB1, 0x22);  // phase length
    //   sendF("ca", 0xBB, 0x17);  // pre-charge voltage
    //   sendF("ca", 0xBE, 0x04);  // VCOMH

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

    display1.fillScreen(0);
    display1.displayBlocking();
    display2.fillScreen(0);
    display2.displayBlocking();

    updateDisplayBox("left", "KBD");
    updateDisplayBox("right", "param8");
}

void showDisplay() {
    display1.fillScreen(0);
    display2.fillScreen(0);

    // Each draw() pushes its display immediately via displayBlocking().
    // display1 faders are drawn first, then display2.
    for (int i : {0, 1, 4, 5}) {
        faders[i]->draw();
    }
    // display1 DMA is complete after all displayBlocking() calls above.
    for (int i : {2, 3, 6, 7}) {
        faders[i]->draw();
    }
    if (deviceLabel[0] != '\0') deviceLabelDirty = true;
    if (bankLabel[0] != '\0') bankLabelDirty = true;
}

void updateFader(int idx, int value) {
    faders[idx]->setValue(value);
}

void updateDisplay() {
    if (screenSaverActive) return;

    if ((left_box_text[0] != '\0' || right_box_text[0] != '\0') && display_needs_update) {
        int text_y = 36;

        if (right_box_text[0] != '\0') {
            display1.fillScreen(0);
            display1.setFont(NULL); // TODO: was u8g2_font_7x14B_tr
            int text_width = getStrWidth(display1, right_box_text);
            int text_x = (256 - text_width) / 2;
            display1.setCursor(text_x, text_y);
            display1.setTextColor(15);
            display1.print(right_box_text);
            display1.displayBlocking();
        }

        if (left_box_text[0] != '\0') {
            display2.fillScreen(0);
            display2.setFont(NULL); // TODO: was u8g2_font_7x14B_tr
            int text_width = getStrWidth(display2, left_box_text);
            int text_x = (256 - text_width) / 2;
            display2.setCursor(text_x, text_y);
            display2.setTextColor(15);
            display2.print(left_box_text);
            display2.displayBlocking();
        }

        display_needs_update = false;
    }

    if (display_active && !staticOverlay && millis() - display_start_time > OVERLAY_TIME) {
        left_box_text[0] = '\0';
        right_box_text[0] = '\0';
        display_needs_update = false;
        display_active = false;
        showDisplay();
    }
    drawDeviceBankLabels();
}

void updateDisplayBox(const char* side, const char* text, bool isStatic) {
    char* target_box;

    if (strcmp(side, "left") == 0) {
        target_box = left_box_text;
    } else if (strcmp(side, "right") == 0) {
        target_box = right_box_text;
    } else {
        return;
    }

    target_box[0] = '\0';
    strncpy(target_box, text, 20);
    target_box[19] = '\0';

    if (isStatic) {
        int text_y = 36;

        if (strcmp(side, "right") == 0) {
            display1.fillScreen(0);
            display1.setFont(NULL); // TODO: was u8g2_font_7x14B_tr
            int text_width = getStrWidth(display1, text);
            int text_x = (256 - text_width) / 2;
            display1.setCursor(text_x, text_y);
            display1.setTextColor(15);
            display1.print(text);
            display1.displayBlocking();
        }

        if (strcmp(side, "left") == 0) {
            display2.fillScreen(0);
            display2.setFont(NULL); // TODO: was u8g2_font_7x14B_tr
            int text_width = getStrWidth(display2, text);
            int text_x = (256 - text_width) / 2;
            display2.setCursor(text_x, text_y);
            display2.setTextColor(15);
            display2.print(text);
            display2.displayBlocking();
        }

        display_needs_update = false;
    } else {
        if (staticOverlay) {
            showDisplay();
        }
        display_needs_update = true;
    }

    display_active = true;
    display_start_time = millis();
    staticOverlay = isStatic;
}

static void drawLabelOverButton(PicoGFX_SSD1322 &disp, const char* txt, int y) {
    int label_w = 64;
    int label_x = (256 - label_w) / 2;
    int inner_w = label_w - 4;
    int area_h = 8;

    disp.fillRect(label_x, y, label_w, area_h, 0);
    disp.fillRect(label_x + 2, y, inner_w, area_h, 15);

    disp.setFont(NULL); // TODO: was u8g2_font_5x8_tr
    int tw = getStrWidth(disp, txt);

    if (tw <= inner_w) {
        int text_x = label_x + (label_w - tw) / 2;
        disp.setCursor(text_x, y + 1);
        disp.setTextColor(0); // black on white background
        disp.print(txt);
    } else {
        char truncated[20];
        strncpy(truncated, txt, sizeof(truncated));
        truncated[sizeof(truncated)-1] = '\0';
        while (strlen(truncated) > 1 && getStrWidth(disp, truncated) > inner_w - 4) {
            truncated[strlen(truncated)-1] = '\0';
        }
        disp.setCursor(label_x + 4, y + 1);
        disp.setTextColor(0);
        disp.print(truncated);
    }
    disp.displayBlocking();
}

void drawDeviceBankLabels() {
    if (screenSaverActive) return;
    if (!deviceLabelDirty && !bankLabelDirty) return;
    if (deviceLabelDirty) {
        deviceLabelDirty = false;
        if (deviceLabel[0] != '\0')
            drawLabelOverButton(display1, deviceLabel, 56);
    }
    if (bankLabelDirty) {
        bankLabelDirty = false;
        if (bankLabel[0] != '\0')
            drawLabelOverButton(display2, bankLabel, 56);
    }
}

void runScreenSaver() {
    static unsigned long lastUpdate = 0;
    static int pixelCount = 0;
    const int pixelsPerFrame = 20;
    const unsigned long frameInterval = 50;

    unsigned long now = millis();
    if (now - lastUpdate >= frameInterval) {
        lastUpdate = now;
        pixelCount = 0;
        display1.fillScreen(0);
        display2.fillScreen(0);
        while (pixelCount < pixelsPerFrame) {
            int x = random(0, 256);
            int y = random(0, 64);
            display1.drawPixel(x, y, 15);
            display2.drawPixel(x, y, 15);
            pixelCount++;
        }
        display1.displayBlocking();
        // display1 DMA complete; SPI is free for display2
        display2.displayBlocking();
    }
}

void setBrightness(uint8_t level) {
    static const uint8_t contrasts[] = { 0x40, 0x80, 0xD0 };
    if (level > 2) level = 2;
    display1.setContrast(contrasts[level]);
    display2.setContrast(contrasts[level]);
    // TODO: SSD1322 master current (cmd 0xC7) has no direct PicoGFX_SSD1322 API.
    // Previously: sendF("ca", 0xC7, currents[level]) where currents = { 0x06, 0x0A, 0x0F }.
}
