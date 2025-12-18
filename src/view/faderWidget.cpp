#include "display.h"

FaderWidget::FaderWidget(U8G2 &u8g2, const char* initialTitle, int x, int y, int boutonNumber)
    : u8g2(u8g2), value(0), x_offset(x), y_offset(y), boutonNumber(boutonNumber)
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

void FaderWidget::drawTitle() {
    // Le titre est affiché en grand, centré verticalement sur le fader
    int area_x = x_offset;
    int area_w = 128;
    // Police plus grande
    u8g2.setFont(u8g2_font_helvB10_tr);
    if(strcmp(paramName, "******") != 0){
        int param_width = u8g2.getStrWidth(paramName);
        int param_height = u8g2.getAscent() - u8g2.getDescent();
        int param_x = x_offset + (128 - param_width) / 2;
        int param_y = y_offset + 12; // centré sur le fader
        // Fond noir sous le texte
        // u8g2.setDrawColor(0);
        // u8g2.drawBox(param_x - 2, param_y - param_height + 2, param_width + 4, param_height);
        // Texte blanc
        u8g2.setDrawColor(1);
        u8g2.setCursor(param_x, param_y);
        u8g2.print(paramName);
    }
}

void FaderWidget::drawFader() {
    int area_x = x_offset;
    int area_y = y_offset + 16; // laisse un peu de marge en haut
    int area_w = 128;
    int area_h = 3; // fader plus haut

    // Efface la zone du fader uniquement
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    int BAR_W = 120;
    int BAR_H = 4; // fader plus haut
    int BAR_X = area_x + 4;
    int BAR_Y = area_y;

    if(strcmp(title, "******") != 0){
        u8g2.setDrawColor(1);
        u8g2.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
        int fillWidth = map(value, 0, 129, 0, BAR_W - 2);
        u8g2.drawBox(BAR_X + 1, BAR_Y + 1, fillWidth, BAR_H);
    }
    // Le titre sera dessiné par-dessus dans drawTitle()
}

// Nouvelle méthode à ajouter dans la classe
void FaderWidget::drawButtonName() {
    // Liste des vrais noms de boutons
    static const char* buttonNames[] = {
        "Prev Track", "Next Track", "Hotswap", "A/B",
        "Prev Device", "Next Device", "Prev Bank", "Next Bank"
    };
    int area_x = x_offset;
    int area_y = y_offset + 20; // sous le fader plus haut
    int area_w = 128;
    int area_h = 12;

    // Choix du nom selon boutonNumber (doit être entre 0 et 7)
    const char* buttonName = "Button ?";
    if (boutonNumber >= 0 && boutonNumber < 8) {
        buttonName = buttonNames[boutonNumber];
    }

    // Efface la zone du bouton uniquement
    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    u8g2.setFont(u8g2_font_6x10_tr);
    int text_width = u8g2.getStrWidth(buttonName);
    int box_x = x_offset + (128 - text_width - 8) / 2;
    int box_y = area_y;
    int box_w = text_width + 8;
    int box_h = 14;

    // Encadré
    u8g2.setDrawColor(1);
    u8g2.drawFrame(x_offset + 4, area_y + 2, 8, 8);

    // Texte centré
    u8g2.setCursor(box_x + 4, box_y + 10);
    u8g2.print(buttonName);
}

void FaderWidget::updateTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if(!display_active){
    drawTitle();
    drawFader();
    // drawButtonName();
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
    drawTitle(); // puis le titre par-dessus
    drawButtonName();

    // Mise à jour de toute la zone
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}


