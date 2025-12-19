#include "display.h"
#include "../core/controls.h"

FaderWidget::FaderWidget(U8G2 &u8g2, const char* initialTitle, int x, int y, int boutonNumber)
    : u8g2(u8g2), value(0), x_offset(x), y_offset(y), boutonNumber(boutonNumber)
{
    strncpy(title, initialTitle, sizeof(title));
    title[sizeof(title)-1] = '\0';
}

void FaderWidget::setTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if(!display_active) drawFader();
}

void FaderWidget::setParamName(const char* txt) {
    strncpy(paramName, txt, sizeof(paramName));
    setTitle(txt);
}

void FaderWidget::setButtonName(const char* txt) {
    strncpy(buttonName, txt, sizeof(buttonName));
    buttonName[sizeof(buttonName)-1] = '\0';
    strncpy(buttonText, txt, sizeof(buttonText));
    if(!display_active) drawButtonName(txt, false);
}

void FaderWidget::updateButtonName(bool state) {
    drawButtonName(buttonText, state);
}

void FaderWidget::showParamName() {
    setTitle(paramName);
}

void FaderWidget::drawTitle() {
    // Le titre est affiché en grand, centré verticalement sur le fader
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 16;

    // Efface la zone du titre uniquement
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    // Police plus grande
    u8g2.setFont(u8g2_font_8x13B_tr);
    if(strcmp(paramName, "******") != 0){
        int param_width = u8g2.getStrWidth(title);
        int param_x = x_offset + (128 - param_width) / 2;
        int param_y = y_offset + ((boutonNumber > 3) ? 14 : 22);
        // Texte blanc
        u8g2.setDrawColor(1);
        u8g2.setCursor(param_x, param_y);
        u8g2.print(title);
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawFader() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    // Efface la zone du fader uniquement
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    int BAR_W = 120;
    int BAR_H = 20;
    int BAR_X = area_x + 4;
    int BAR_Y = area_y + 1;

    if(strcmp(title, "******") != 0){
        // Active le mode transparent pour le texte
        u8g2.setFontMode(1);
        
        // Dessine le cadre du fader
        u8g2.setDrawColor(1);
        u8g2.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
        
        // Dessine le remplissage du fader
        int fillWidth = map(value, 0, 127, 0, BAR_W);
        u8g2.drawBox(BAR_X, BAR_Y, fillWidth, BAR_H);
        
        // Dessine le titre centré dans le fader en mode XOR
        u8g2.setFont(u8g2_font_8x13B_tr);
        int text_width = u8g2.getStrWidth(title);
        int text_x = BAR_X + (BAR_W - text_width) / 2;
        int text_y = BAR_Y + 14; // centré verticalement dans la barre
        
        u8g2.setDrawColor(2); // mode XOR : blanc sur noir, noir sur blanc
        u8g2.drawStr(text_x, text_y, title);
        
        // Repasse en mode normal
        u8g2.setFontMode(0);
        u8g2.setDrawColor(1);
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

// Nouvelle méthode à ajouter dans la classe
void FaderWidget::drawButtonName(const char* txt, bool state) {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 24 : 0); // sous le fader plus haut
    int area_w = 128;
    int area_h = 8;
    int box_width = 120;
    
    // Efface la zone du bouton uniquement
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    u8g2.setFont(u8g2_font_5x8_tr);
    int text_width = u8g2.getStrWidth(txt);
    int box_x = x_offset + (128 - text_width - 8) / 2;

    // Encadré
    if(state){
    u8g2.setDrawColor(1);
    u8g2.drawBox(area_x + 4, area_y , box_width, area_h);
    u8g2.setDrawColor(0); // 2 = gris moyen sur certains écrans
    // u8g2.setDrawColor(1);
    // u8g2.drawFrame(box_x - 8, area_y + 1, 6, 6);
    }
    else{
    u8g2.setDrawColor(3); // 2 = gris moyen sur certains écrans
    }   

    // Texte centré, en gris si supporté
    // u8g2.setDrawColor(1); // 2 = gris moyen sur certains écrans
    u8g2.setCursor(box_x + 4, area_y + 7);
    u8g2.print(txt);
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::updateTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    Serial.print("Texte: ");
    Serial.println(title);
    if(!display_active){
    // drawTitle();
    drawFader();
    }
}

void FaderWidget::setValue(int val) {
    value = constrain(val, 0, 127);
    if(!display_active)drawFader();
    static char title[20];
    snprintf(title, sizeof(title), "%d", value);
    // updateTitle(title);
}

void FaderWidget::setEmpty() {
    value = 0;
    draw();
}

void FaderWidget::draw() {
    // Efface toute la zone du widget (titre + fader + bouton)
    int area_x = x_offset;
    int area_y = y_offset;
    int area_w = 128;
    int area_h = 44;
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    drawFader(); // d'abord le fader
    // drawTitle(); // puis le titre par-dessus
    updateButtonName(false);

    // Mise à jour de toute la zone
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}


