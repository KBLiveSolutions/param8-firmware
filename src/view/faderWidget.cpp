#include "display.h"
#include "../core/controls.h"

FaderLayout faderLayout = LAYOUT_COMPACT;

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
    paramName[sizeof(paramName)-1] = '\0';
    if(faderLayout == LAYOUT_COMPACT || faderLayout == LAYOUT_STACKED) {
        title[0] = '\0';
    } else {
        strncpy(title, txt, sizeof(title));
        title[sizeof(title)-1] = '\0';
    }
    if(!display_active) drawFader();
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
    if(faderLayout == LAYOUT_DYNAMIC)
        setTitle(paramName);
    else
        if(!display_active) drawFader();
}

void FaderWidget::drawTitle() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 16;

    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    u8g2.setFont(u8g2_font_8x13B_tr);
    if(strcmp(paramName, "******") != 0){
        int param_width = u8g2.getStrWidth(title);
        int param_x = x_offset + (128 - param_width) / 2;
        int param_y = y_offset + ((boutonNumber > 3) ? 8 : 16);
        u8g2.setDrawColor(1);
        u8g2.setCursor(param_x, param_y);
        u8g2.print(title);
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawFader() {
    if(faderLayout == LAYOUT_COMPACT)
        drawFaderCompact();
    else if(faderLayout == LAYOUT_STACKED)
        drawFaderStacked();
    else
        drawFaderDynamic();
}

void FaderWidget::drawFaderDynamic() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    int BAR_W = 120;
    int BAR_H = 5;
    int BAR_X = area_x + 4;
    int BAR_Y = area_y + 1;
    int FADER_H = 5;

    if(strcmp(title, "******") != 0){
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);

        int fillWidth = map(value, 0, 127, 0, BAR_W - 2);
        int fader_y = BAR_Y + BAR_H - FADER_H - 1 + 17;
        u8g2.drawFrame(BAR_X, fader_y, BAR_W, BAR_H);
        u8g2.drawBox(BAR_X + 1, fader_y, fillWidth, FADER_H);

        u8g2.setFont(u8g2_font_8x13B_tr);
        int text_width = u8g2.getStrWidth(title);
        int text_x = BAR_X + (BAR_W - text_width) / 2;
        int text_y = BAR_Y + 12;

        u8g2.setDrawColor(1);
        u8g2.drawStr(text_x, text_y, title);

        u8g2.setFontMode(0);
        u8g2.setDrawColor(1);
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawFaderCompact() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    if(strcmp(paramName, "******") != 0){
        u8g2.setDrawColor(1);

        int MARGIN = 4;
        int GAP = 2;
        int TPAD = 2;
        int col_name_w = 48;
        int col_pie_w  = 20;
        int col_val_w  = 48;
        int col_name_x = area_x + MARGIN;
        int col_pie_x  = col_name_x + col_name_w + GAP;
        int col_val_x  = col_pie_x + col_pie_w + GAP;

        // --- Left: param name, right-justified, 2 lines max ---
        u8g2.setFont(u8g2_font_helvB08_te); // u8g2_font_luBS08_te
        int charW = 5;
        int textW = col_name_w - TPAD * 2;
        int maxChars = textW / charW;
        int len = strlen(paramName);

        if(len <= maxChars) {
            int w = u8g2.getStrWidth(paramName);
            u8g2.drawStr(col_name_x + col_name_w - TPAD - w, area_y + 14, paramName);
        } else {
            char line1[21], line2[21];
            int brk = -1;
            for(int i = 0; i < len && i < maxChars; i++)
                if(paramName[i] == ' ') brk = i;
            if(brk > 0) {
                strncpy(line1, paramName, brk);
                line1[brk] = '\0';
                strncpy(line2, paramName + brk + 1, sizeof(line2) - 1);
                line2[sizeof(line2)-1] = '\0';
            } else {
                strncpy(line1, paramName, maxChars);
                line1[maxChars] = '\0';
                strncpy(line2, paramName + maxChars, sizeof(line2) - 1);
                line2[sizeof(line2)-1] = '\0';
            }
            if((int)strlen(line2) > maxChars) line2[maxChars] = '\0';

            int w1 = u8g2.getStrWidth(line1);
            int w2 = u8g2.getStrWidth(line2);
            u8g2.drawStr(col_name_x + col_name_w - TPAD - w1, area_y + 10, line1);
            u8g2.drawStr(col_name_x + col_name_w - TPAD - w2, area_y + 21, line2);
        }
        // --- Center: knob arc indicator ---
        int pie_cx = col_pie_x + col_pie_w / 2;
        int pie_cy = area_y + area_h / 2;
        int arc_r = 9;

        float start_rad = 2.3562f;   // 135° = 8h
        float sweep = 4.7124f;       // 270°
        float val_rad = start_rad + (value / 127.0f) * sweep;

        // Full track arc (8h to 4h)
        u8g2.setDrawColor(1);
        for(float a = start_rad; a <= start_rad + sweep; a += 0.05f) {
            u8g2.drawPixel(pie_cx + (int)roundf(cosf(a) * arc_r),
                           pie_cy + (int)roundf(sinf(a) * arc_r));
        }

        // Needle from center outward at value angle
        int needle_inner = 3;
        int needle_outer = arc_r - 1;
        for(int r = needle_inner; r <= needle_outer; r++) {
            u8g2.drawPixel(pie_cx + (int)roundf(cosf(val_rad) * r),
                           pie_cy + (int)roundf(sinf(val_rad) * r));
        }
        // --- Right: param value, left-justified, 2 lines max ---
        u8g2.setFont(u8g2_font_6x10_tf);
        int valMaxChars = (col_val_w - TPAD * 2) / charW;
        int vlen = strlen(title);

        if(vlen <= valMaxChars) {
            u8g2.drawStr(col_val_x + TPAD, area_y + 14, title);
        } else {
            char vl1[21], vl2[21];
            int vbrk = -1;
            for(int i = 0; i < vlen && i < valMaxChars; i++)
                if(title[i] == ' ') vbrk = i;
            if(vbrk > 0) {
                strncpy(vl1, title, vbrk);
                vl1[vbrk] = '\0';
                strncpy(vl2, title + vbrk + 1, sizeof(vl2) - 1);
                vl2[sizeof(vl2)-1] = '\0';
            } else {
                strncpy(vl1, title, valMaxChars);
                vl1[valMaxChars] = '\0';
                strncpy(vl2, title + valMaxChars, sizeof(vl2) - 1);
                vl2[sizeof(vl2)-1] = '\0';
            }
            if((int)strlen(vl2) > valMaxChars) vl2[valMaxChars] = '\0';
            u8g2.drawStr(col_val_x + TPAD, area_y + 9, vl1);
            u8g2.drawStr(col_val_x + TPAD, area_y + 18, vl2);
        }
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawFaderStacked() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    if(strcmp(paramName, "******") != 0){
        u8g2.setDrawColor(1);
        int PAD = 4;

        // --- Row 1: param name centered (10px) ---
        u8g2.setFont(u8g2_font_helvB08_te);
        int name_w = u8g2.getStrWidth(paramName);
        int name_x = area_x + (area_w - name_w) / 2;
        if(name_x < area_x + PAD) name_x = area_x + PAD;
        u8g2.drawStr(name_x, area_y + 9, paramName);

        // --- Row 2: horizontal slider ---
        int bar_w = area_w * 80 / 100;
        int bar_x = area_x + (area_w - bar_w) / 2;
        int bar_h = 4;
        int bar_y = area_y + 12;
        u8g2.drawFrame(bar_x, bar_y, bar_w, bar_h);
        int fillW = map(value, 0, 127, 0, bar_w - 2);
        if(fillW > 0)
            u8g2.drawBox(bar_x + 1, bar_y + 1, fillW, bar_h - 2);

        // --- Row 3: param value centered (8px) ---
        u8g2.setFont(u8g2_font_5x7_tr);
        int val_w = u8g2.getStrWidth(title);
        int val_x = area_x + (area_w - val_w) / 2;
        u8g2.drawStr(val_x, area_y + 23, title);
    }
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawButtonName(const char* txt, bool state) {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 24 : 0); // sous le fader plus haut
    int area_w = 128;
    int area_h = 8;
    int box_width = 124;
    
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


