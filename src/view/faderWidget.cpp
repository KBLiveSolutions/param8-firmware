#include "display.h"

FaderWidget::FaderWidget(U8G2 &u8g2, const char* initialTitle, int x, int y)
    : u8g2(u8g2), value(0), x_offset(x), y_offset(y)
{
    strncpy(title, initialTitle, sizeof(title));
    title[sizeof(title)-1] = '\0';
}

void FaderWidget::setTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if(!display_active) drawTitle();
}

void FaderWidget::setParamName(const char* txt) {
    strncpy(paramName, txt, sizeof(paramName));
    setTitle(txt);
}

void FaderWidget::showParamName() {
    setTitle(paramName);
}

void FaderWidget::drawFader() {     
        int area_x = x_offset;
        int area_y = y_offset + 16;
        int area_w = 128;
        int area_h = 16; // +2 pour un peu d'espace sous le texte

        // Définir la zone à mettre à jour (par exemple, une barre de 100x20 à la position (20,20))

        int BAR_W = 120;
        int BAR_H = 10;
        int VERTICAL_OFFSET = 3;
        int HORIZONTAL_OFFSET = 4;
        int BAR_X = area_x + HORIZONTAL_OFFSET;
        int BAR_Y = area_y + VERTICAL_OFFSET;

        // Efface la zone dans le buffer
        u8g2.setDrawColor(0);
        u8g2.drawBox(BAR_X, BAR_Y, BAR_W, BAR_H);

        if(strcmp(title, "******") != 0){
            // Dessine la nouvelle barre
            u8g2.setDrawColor(1);
            u8g2.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
            int fillWidth = map(value, 0, 129, 0, BAR_W - 2);
            u8g2.drawBox(BAR_X + 2, BAR_Y + 2, fillWidth, BAR_H - 4);
        }

        // Mets à jour uniquement la zone de la barre
        u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::updateTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if(!display_active){
    drawTitle();
    drawFader();
    }
}

void FaderWidget::drawTitle() {
        int area_x = x_offset;
        int area_y = y_offset;
        int area_w = 128;
        int area_h = 16; // +2 pour un peu d'espace sous le texte
        u8g2.setDrawColor(0);
        u8g2.drawBox(x_offset, y_offset, area_w, area_h);
        if(strcmp(title, "******") != 0){
            u8g2.setDrawColor(1);
            u8g2.setFont(u8g2_font_helvB10_tf); //u8g2_font_7x14B_tr  u8g2_font_7x14_tf u8g2_font_helvB12_te
            int text_width = u8g2.getStrWidth(title);
            int text_x = x_offset + (area_w - text_width) / 2;
            int text_y = y_offset + u8g2.getAscent() - 2;

            u8g2.setCursor(text_x, text_y + 4); // +2 pour un peu d'espace sous le texte
            u8g2.print(title);
        }
        u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
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
    drawTitle();
    drawFader();
}


