#include <Arduino.h>
#include "display.h"
#include "../core/actions.h"
#include "../core/controls.h"


FaderWidget* faders[8];


char left_box_text[20] = {"param8"};
char right_box_text[20] = {"KBD"};
unsigned long display_start_time = 0;
bool display_active = false;
bool display_needs_update = false;
bool staticOverlay = false; // Flag pour l'affichage statique
bool screenSaverActive = false;

void setupDisplay() {
    u8g2.begin();
    u8g2_2.begin();

    // u8g2 : 1 2 / 5 6
    faders[0] = new FaderWidget(u8g2,  "Fader 1",   0,   0, 0);    // Bloc 1
    faders[1] = new FaderWidget(u8g2,  "Fader 2", 128,   0, 1);    // Bloc 2
    faders[4] = new FaderWidget(u8g2,  "Fader 5",   0,  32, 4);    // Bloc 5
    faders[5] = new FaderWidget(u8g2,  "Fader 6", 128,  32, 5);    // Bloc 6

    // u8g2_2 : 3 4 / 7 8
    faders[2] = new FaderWidget(u8g2_2, "Fader 3",   0,   0, 2);   // Bloc 3
    faders[3] = new FaderWidget(u8g2_2, "Fader 4", 128,   0, 3);   // Bloc 4
    faders[6] = new FaderWidget(u8g2_2, "Fader 7",   0,  32, 6);   // Bloc 7
    faders[7] = new FaderWidget(u8g2_2, "Fader 8", 128,  32, 7);   // Bloc 8
    u8g2.clearBuffer();
    u8g2_2.clearBuffer();
    updateDisplayBox("left", "KBD");
    updateDisplayBox("right", "param8");
}

void showDisplay() {
    u8g2.clearBuffer();
    u8g2_2.clearBuffer();

    for (int i : {0, 1, 4, 5}) {
        faders[i]->draw();
    }
    for (int i : {2, 3, 6, 7}) {
        faders[i]->draw();
    }
}

void updateFader(int idx, int value) {
    faders[idx]->setValue(value);
}

void updateDisplay() {
    // Afficher si au moins une boîte a du contenu ET que l'affichage doit être mis à jour
    if ((left_box_text[0] != '\0' || right_box_text[0] != '\0') && display_needs_update) {    
        int area_x = 64;
        int area_y = 16; 
        int area_w = 128;
        int area_h = 32;
        int text_y = 36;

        // Afficher la boîte droite si elle a du contenu
        if (right_box_text[0] != '\0') {
            u8g2.clearBuffer();
            u8g2.setDrawColor(1);
            u8g2.setFont(u8g2_font_7x14B_tr);
            int text_width = u8g2.getStrWidth(right_box_text);
            int text_x = (256 - text_width) / 2;
            u8g2.setCursor(text_x, text_y);
            u8g2.print(right_box_text);
            u8g2.sendBuffer();
        }

        // Afficher la boîte gauche si elle a du contenu
        if (left_box_text[0] != '\0') {
            u8g2_2.clearBuffer();
            u8g2_2.setDrawColor(1);
            u8g2_2.setFont(u8g2_font_7x14B_tr);
            int text_width = u8g2_2.getStrWidth(left_box_text);
            int text_x = (256 - text_width) / 2;
            u8g2_2.setCursor(text_x, text_y);
            u8g2_2.print(left_box_text);
            u8g2_2.sendBuffer();
        }

        display_needs_update = false;
    }
    
    // Ne pas effacer les overlays si staticOverlay est true
    if (display_active && !staticOverlay && millis() - display_start_time > OVERLAY_TIME) {
        left_box_text[0] = '\0';
        right_box_text[0] = '\0';
        display_needs_update = false;
        display_active = false;
        showDisplay();
    }
}

void updateDisplayBox(const char* side, const char* text, bool isStatic) {
    char* target_box;
    
    if (strcmp(side, "left") == 0) {
        target_box = left_box_text;
    } else if (strcmp(side, "right") == 0) {
        target_box = right_box_text;
    } else {
        return; // Invalid side parameter
    }
    
    target_box[0] = '\0';
    strncpy(target_box, text, 20);
    target_box[19] = '\0'; // Ensure null termination
    
    // Si c'est un overlay statique, effacer et dessiner immédiatement
    if (isStatic) {
        int area_y = 16;
        int area_h = 32;
        int text_y = 36;

        if (strcmp(side, "right") == 0) {
            u8g2.clearBuffer();
            u8g2.setDrawColor(1);
            u8g2.setFont(u8g2_font_7x14B_tr);
            int text_width = u8g2.getStrWidth(text);
            int text_x = (256 - text_width) / 2;
            u8g2.setCursor(text_x, text_y);
            u8g2.print(text);
            u8g2.sendBuffer();
        }

        if (strcmp(side, "left") == 0) {
            u8g2_2.clearBuffer();
            u8g2_2.setDrawColor(1);
            u8g2_2.setFont(u8g2_font_7x14B_tr);
            int text_width = u8g2_2.getStrWidth(text);
            int text_x = (256 - text_width) / 2;
            u8g2_2.setCursor(text_x, text_y);
            u8g2_2.print(text);
            u8g2_2.sendBuffer();
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

void runScreenSaver() {
    static unsigned long lastUpdate = 0;
    static int pixelCount = 0;
    const int pixelsPerFrame = 20; // Moins de pixels par frame
    const unsigned long frameInterval = 50; // ms entre chaque frame

    unsigned long now = millis();
    if (now - lastUpdate >= frameInterval) {
        lastUpdate = now;
        pixelCount = 0;
        u8g2.clearBuffer();
        u8g2_2.clearBuffer();
        while (pixelCount < pixelsPerFrame) {
            int x = random(0, 256);
            int y = random(0, 64);
            u8g2.drawPixel(x, y);
            u8g2_2.drawPixel(x, y);
            pixelCount++;
        }
        u8g2.sendBuffer();
        u8g2_2.sendBuffer();
    }
}
