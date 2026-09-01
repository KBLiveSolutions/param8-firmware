#include "display.h"
#include "../core/controls.h"

FaderLayout faderLayout = LAYOUT_COMPACT;

void FaderWidget::drawSeparators() {
    u8g2.setDrawColor(1);
    u8g2.drawVLine(128, y_offset, 32);
    // u8g2.drawHLine(x_offset, 32, 128);
}

FaderWidget::FaderWidget(U8G2 &u8g2, const char* initialTitle, int x, int y, int boutonNumber)
    : u8g2(u8g2), value(0), x_offset(x), y_offset(y), boutonNumber(boutonNumber)
{
    strncpy(title, initialTitle, sizeof(title));
    title[sizeof(title)-1] = '\0';
    paramName[0] = '\0';
    buttonName[0] = '\0';
    buttonText[0] = '\0';
}

void FaderWidget::setTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if(!display_active) drawFader();
}

void FaderWidget::setParamName(const char* txt) {
    strncpy(paramName, txt, sizeof(paramName));
    paramName[sizeof(paramName)-1] = '\0';
    if (!valueOnly) {
        showingValue = false;
        if(faderLayout == LAYOUT_DYNAMIC) {
            strncpy(title, txt, sizeof(title));
            title[sizeof(title)-1] = '\0';
        }
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
    if (valueOnly) return;
    showingValue = false;
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
    drawSeparators();
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

    const char* displayText = showingValue ? title : paramName;

    if(strcmp(paramName, "******") != 0){
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);

        int fillWidth = map(value, 0, 127, 0, BAR_W - 2);
        int fader_y = BAR_Y + BAR_H - FADER_H - 1 + 17;
        u8g2.drawFrame(BAR_X, fader_y, BAR_W, BAR_H);
        u8g2.drawBox(BAR_X + 1, fader_y, fillWidth, FADER_H);

        u8g2.setFont(u8g2_font_8x13B_tr);
        int text_width = u8g2.getStrWidth(displayText);
        int text_y = BAR_Y + 12;

        if (text_width <= BAR_W) {
            int text_x = BAR_X + (BAR_W - text_width) / 2;
            u8g2.setDrawColor(1);
            u8g2.drawStr(text_x, text_y, displayText);
        } else {
            char truncated[20];
            strncpy(truncated, displayText, sizeof(truncated));
            truncated[sizeof(truncated)-1] = '\0';
            while (strlen(truncated) > 1 && u8g2.getStrWidth(truncated) > BAR_W) {
                truncated[strlen(truncated)-1] = '\0';
            }
            u8g2.setDrawColor(1);
            u8g2.drawStr(BAR_X, text_y, truncated);
        }

        u8g2.setFontMode(0);
        u8g2.setDrawColor(1);
    }
    drawSeparators();
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawFaderCompact() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    u8g2.setDrawColor(0);
    u8g2.drawBox(area_x, area_y, area_w, area_h);

    bool inactive = (strcmp(title, "---") == 0);

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
        u8g2.setFont(u8g2_font_helvB08_te);
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

        float start_rad = 2.3562f;   // 135°
        float sweep = 4.7124f;       // 270°
        float val_rad = start_rad + (value / 127.0f) * sweep;

        // Arc: draw full circle then erase the 90° gap at bottom-right
        u8g2.setDrawColor(1);
        u8g2.drawCircle(pie_cx, pie_cy, arc_r);
        u8g2.setDrawColor(0);
        {
            float gap_start = 0.7854f;  // 45°
            float gap_end = start_rad;   // 135°
            for(float a = gap_start; a <= gap_end; a += 0.02f) {
                int px = pie_cx + (int)roundf(cosf(a) * arc_r);
                int py = pie_cy + (int)roundf(sinf(a) * arc_r);
                u8g2.drawPixel(px, py);
            }
        }
        u8g2.setDrawColor(1);

        if(!inactive) {
            // Needle: quantized to 48 positions
            int needle_step = (int)roundf(value * 48.0f / 127.0f);
            float q_rad = start_rad + (needle_step / 48.0f) * sweep;
            int nx2 = pie_cx + (int)roundf(cosf(q_rad) * (arc_r - 2));
            int ny2 = pie_cy + (int)roundf(sinf(q_rad) * (arc_r - 2));
            u8g2.drawLine(pie_cx, pie_cy, nx2, ny2);
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
    drawSeparators();
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
    drawSeparators();
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}

void FaderWidget::drawButtonName(const char* txt, bool state) {
    int area_y = y_offset + ((boutonNumber > 3) ? 24 : 0);
    int area_h = 8;
    int btn_x = x_offset + 32;
    int btn_w = 64;

    // Clear only our side (avoid the center label zone)
    int clear_x, clear_w;
    if (x_offset == 0) {
        clear_x = 0;
        clear_w = 96;
    } else {
        clear_x = 160;
        clear_w = 96;
    }
    u8g2.setDrawColor(0);
    u8g2.drawBox(clear_x, area_y, clear_w, area_h);

    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(txt);

    if (state) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(btn_x, area_y, btn_w, area_h);
        u8g2.setDrawColor(0);
    } else {
        u8g2.setDrawColor(3);
    }

    if (tw <= btn_w - 4) {
        int text_x = btn_x + (btn_w - tw) / 2;
        u8g2.setCursor(text_x, area_y + 7);
        u8g2.print(txt);
    } else {
        u8g2.setCursor(btn_x + 2, area_y + 7);
        char truncated[20];
        strncpy(truncated, txt, sizeof(truncated));
        truncated[sizeof(truncated)-1] = '\0';
        while (strlen(truncated) > 1 && u8g2.getStrWidth(truncated) > btn_w - 4) {
            truncated[strlen(truncated)-1] = '\0';
        }
        u8g2.print(truncated);
    }

    u8g2.setDrawColor(1);
    drawSeparators();
    u8g2.updateDisplayArea(clear_x / 8, area_y / 8, clear_w / 8, 1);
}

void FaderWidget::updateTitle(const char* txt) {
    strncpy(title, txt, sizeof(title));
    title[sizeof(title)-1] = '\0';
    if (valueOnly)
        showingValue = true;
    if(!display_active){
    drawFader();
    }
}

void FaderWidget::setValue(int val) {
    value = constrain(val, 0, 127);
    if(!display_active) drawFader();
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

    drawSeparators();
    u8g2.updateDisplayArea(area_x / 8, area_y / 8, area_w / 8, area_h / 8);
}


