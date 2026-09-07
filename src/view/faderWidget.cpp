#include "display.h"
#include "../core/controls.h"

// NOTE: Text y-coordinates were baseline (bottom) in U8g2 but are top-left in
// Adafruit GFX. Positions are kept as-is for the initial port — visual alignment
// will be off by ~font_ascent pixels until fonts are tuned. TODO: adjust y values.

// NOTE: All U8g2 fonts replaced with the Adafruit GFX built-in 5×7 font (setFont(NULL)).
// TODO: port U8g2 fonts to Adafruit GFX-compatible fonts for proper sizing.

FaderLayout faderLayout = LAYOUT_COMPACT;

void FaderWidget::drawSeparators() {
}

FaderWidget::FaderWidget(PicoGFX_SSD1322 &display, const char* initialTitle, int x, int y, int boutonNumber)
    : display(display), value(0), x_offset(x), y_offset(y), boutonNumber(boutonNumber)
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
    dimmed = (txt[0] == '~');
    const char* name = dimmed ? txt + 1 : txt;
    strncpy(paramName, name, sizeof(paramName));
    paramName[sizeof(paramName)-1] = '\0';
    disabled = (strcmp(name, "---") == 0);
    if (!valueOnly && !showingValue) {
        if(faderLayout == LAYOUT_DYNAMIC) {
            strncpy(title, name, sizeof(title));
            title[sizeof(title)-1] = '\0';
        }
    }
}

void FaderWidget::setButtonName(const char* txt) {
    strncpy(buttonName, txt, sizeof(buttonName));
    buttonName[sizeof(buttonName)-1] = '\0';
    strncpy(buttonText, txt, sizeof(buttonText));
    if(!display_active) drawButtonName(txt, false);
}

void FaderWidget::updateButtonName(bool state) {
    buttonState = state;
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

    display.fillRect(area_x, area_y, area_w, area_h, 0);

    display.setFont(NULL); // TODO: was u8g2_font_8x13B_tr
    if(strcmp(paramName, "******") != 0){
        int param_width = getStrWidth(display, title);
        int param_x = x_offset + (128 - param_width) / 2;
        int param_y = y_offset + ((boutonNumber > 3) ? 8 : 16);
        display.setCursor(param_x, param_y);
        display.setTextColor(15);
        display.print(title);
    }
    drawSeparators();
    display.displayBlocking();
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

    display.fillRect(area_x, area_y, area_w, area_h, 0);
    if (strcmp(paramName, "---") == 0) {
        display.displayBlocking();
        return;
    }

    int BAR_W = 100;
    int BAR_H = 5;
    int BAR_X = area_x + (area_w - BAR_W) / 2;
    int BAR_Y = area_y + 1;
    int FADER_H = 5;

    const char* displayText = showingValue ? title : paramName;

    if(strcmp(paramName, "******") != 0){
        {
            int fillWidth = map(value, 0, 127, 0, BAR_W - 2);
            int fader_y = BAR_Y + BAR_H - FADER_H - 1 + 17;
            display.drawRect(BAR_X, fader_y, BAR_W, BAR_H, 15);
            if (!dimmed)
                display.fillRect(BAR_X + 1, fader_y, fillWidth, FADER_H, 15);
        }

        display.setFont(NULL); // TODO: was u8g2_font_8x13B_tr
        int text_width = getStrWidth(display, displayText);
        int text_y = BAR_Y + 12;
        int TEXT_MAX = area_w - 8;

        if (text_width <= TEXT_MAX) {
            int text_x = area_x + (area_w - text_width) / 2;
            display.setCursor(text_x, text_y);
            display.setTextColor(15);
            display.print(displayText);
        } else {
            char truncated[20];
            strncpy(truncated, displayText, sizeof(truncated));
            truncated[sizeof(truncated)-1] = '\0';
            while (strlen(truncated) > 1 && getStrWidth(display, truncated) > TEXT_MAX) {
                truncated[strlen(truncated)-1] = '\0';
            }
            display.setCursor(area_x + 4, text_y);
            display.setTextColor(15);
            display.print(truncated);
        }
    }
    drawSeparators();
    display.displayBlocking();
}

void FaderWidget::drawFaderCompact() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    display.fillRect(area_x, area_y, area_w, area_h, 0);
    if (strcmp(paramName, "---") == 0) {
        display.displayBlocking();
        return;
    }

    bool inactive = disabled || dimmed;

    if(strcmp(paramName, "******") != 0){
        int MARGIN = 6;
        int GAP = 2;
        int TPAD = 2;
        int col_name_w = 48;
        int col_pie_w  = 20;
        int col_val_w  = 48;
        int col_name_x = area_x + MARGIN;
        int col_pie_x  = col_name_x + col_name_w + GAP;
        int col_val_x  = col_pie_x + col_pie_w + GAP;

        // --- Left: param name, right-justified, 2 lines max ---
        display.setFont(NULL); // TODO: was u8g2_font_helvB08_te
        int charW = 5;
        int textW = col_name_w - TPAD * 2;
        int maxChars = textW / charW;
        int len = strlen(paramName);

        display.setTextColor(15);
        if(len <= maxChars) {
            int w = getStrWidth(display, paramName);
            display.setCursor(col_name_x + col_name_w - TPAD - w, area_y + 14);
            display.print(paramName);
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

            int w1 = getStrWidth(display, line1);
            int w2 = getStrWidth(display, line2);
            display.setCursor(col_name_x + col_name_w - TPAD - w1, area_y + 10);
            display.print(line1);
            display.setCursor(col_name_x + col_name_w - TPAD - w2, area_y + 21);
            display.print(line2);
        }

        // --- Center: knob arc indicator ---
        int pie_cx = col_pie_x + col_pie_w / 2;
        int pie_cy = area_y + area_h / 2;
        int arc_r = 9;

        float start_rad = 2.3562f;   // 135°
        float sweep = 4.7124f;       // 270°
        float val_rad = start_rad + (value / 127.0f) * sweep;

        display.drawCircle(pie_cx, pie_cy, arc_r, 15);
        // Erase the 90° gap at the bottom of the arc
        {
            float gap_start = 0.7854f;  // 45°
            float gap_end = start_rad;  // 135°
            for(float a = gap_start; a <= gap_end; a += 0.02f) {
                int px = pie_cx + (int)roundf(cosf(a) * arc_r);
                int py = pie_cy + (int)roundf(sinf(a) * arc_r);
                display.drawPixel(px, py, 0);
            }
        }

        if(!inactive) {
            // Needle: quantized to 48 positions
            int needle_step = (int)roundf(value * 48.0f / 127.0f);
            float q_rad = start_rad + (needle_step / 48.0f) * sweep;
            int nx2 = pie_cx + (int)roundf(cosf(q_rad) * (arc_r - 2));
            int ny2 = pie_cy + (int)roundf(sinf(q_rad) * (arc_r - 2));
            display.drawLine(pie_cx, pie_cy, nx2, ny2, 15);
        }

        // --- Right: param value, left-justified, 2 lines max ---
        display.setFont(NULL); // TODO: was u8g2_font_6x10_tf
        const char* valText = disabled ? "---" : title;
        int valMaxChars = (col_val_w - TPAD * 2) / charW;
        int vlen = strlen(valText);

        display.setTextColor(15);
        if(vlen <= valMaxChars) {
            display.setCursor(col_val_x + TPAD, area_y + 14);
            display.print(valText);
        } else {
            char vl1[21], vl2[21];
            int vbrk = -1;
            for(int i = 0; i < vlen && i < valMaxChars; i++)
                if(valText[i] == ' ') vbrk = i;
            if(vbrk > 0) {
                strncpy(vl1, valText, vbrk);
                vl1[vbrk] = '\0';
                strncpy(vl2, valText + vbrk + 1, sizeof(vl2) - 1);
                vl2[sizeof(vl2)-1] = '\0';
            } else {
                strncpy(vl1, valText, valMaxChars);
                vl1[valMaxChars] = '\0';
                strncpy(vl2, valText + valMaxChars, sizeof(vl2) - 1);
                vl2[sizeof(vl2)-1] = '\0';
            }
            if((int)strlen(vl2) > valMaxChars) vl2[valMaxChars] = '\0';
            display.setCursor(col_val_x + TPAD, area_y + 9);
            display.print(vl1);
            display.setCursor(col_val_x + TPAD, area_y + 18);
            display.print(vl2);
        }
    }
    drawSeparators();
    display.displayBlocking();
}

void FaderWidget::drawFaderStacked() {
    int area_x = x_offset;
    int area_y = y_offset + ((boutonNumber > 3) ? 0 : 8);
    int area_w = 128;
    int area_h = 24;

    display.fillRect(area_x, area_y, area_w, area_h, 0);
    if (strcmp(paramName, "---") == 0) {
        display.displayBlocking();
        return;
    }

    if(strcmp(paramName, "******") != 0){
        int PAD = 4;

        // --- Row 1: param name centered ---
        display.setFont(NULL); // TODO: was u8g2_font_helvB08_te
        int name_w = getStrWidth(display, paramName);
        int name_x = area_x + (area_w - name_w) / 2;
        if(name_x < area_x + PAD) name_x = area_x + PAD;
        display.setCursor(name_x, area_y + 9);
        display.setTextColor(15);
        display.print(paramName);

        if(!disabled) {
            // --- Row 2: horizontal slider ---
            int bar_w = area_w * 80 / 100;
            int bar_x = area_x + (area_w - bar_w) / 2;
            int bar_h = 4;
            int bar_y = area_y + 12;
            display.drawRect(bar_x, bar_y, bar_w, bar_h, 15);
            if(!dimmed) {
                int fillW = map(value, 0, 127, 0, bar_w - 2);
                if(fillW > 0)
                    display.fillRect(bar_x + 1, bar_y + 1, fillW, bar_h - 2, 15);
            }
        }

        // --- Row 3: param value centered ---
        display.setFont(NULL); // TODO: was u8g2_font_5x7_tr
        const char* valText = disabled ? "---" : title;
        int val_w = getStrWidth(display, valText);
        int val_x = area_x + (area_w - val_w) / 2;
        display.setCursor(val_x, area_y + 23);
        display.setTextColor(15);
        display.print(valText);
    }
    drawSeparators();
    display.displayBlocking();
}

void FaderWidget::drawButtonName(const char* txt, bool state) {
    char upper[20];
    for (size_t i = 0; txt[i] && i < sizeof(upper) - 1; i++) {
        upper[i] = toupper(txt[i]);
        upper[i + 1] = '\0';
    }
    txt = upper;

    int area_y = y_offset + ((boutonNumber > 3) ? 24 : 0);
    int area_h = 8;
    int btn_x = x_offset + 32;
    int btn_w = 64;

    int clear_x, clear_w;
    if (x_offset == 0) {
        clear_x = 0;
        clear_w = 96;
    } else {
        clear_x = 160;
        clear_w = 96;
    }
    display.fillRect(clear_x, area_y, clear_w, area_h, 0);

    display.setFont(NULL); // TODO: was u8g2_font_5x8_tr
    int tw = getStrWidth(display, txt);

    int textColor;
    if (state) {
        display.fillRect(btn_x, area_y, btn_w, area_h, 15);
        textColor = 0; // black text on white background
    } else {
        textColor = 15; // white text on black background (was XOR/color-3 in U8g2)
    }

    if (tw <= btn_w - 4) {
        int text_x = btn_x + (btn_w - tw) / 2;
        display.setCursor(text_x, area_y + 1);
        display.setTextColor(textColor);
        display.print(txt);
    } else {
        char truncated[20];
        strncpy(truncated, txt, sizeof(truncated));
        truncated[sizeof(truncated)-1] = '\0';
        while (strlen(truncated) > 1 && getStrWidth(display, truncated) > btn_w - 4) {
            truncated[strlen(truncated)-1] = '\0';
        }
        display.setCursor(btn_x + 2, area_y + 1);
        display.setTextColor(textColor);
        display.print(truncated);
    }

    display.drawRect(btn_x, area_y, btn_w, area_h, 15);
    drawSeparators();
    display.displayBlocking();
}

void FaderWidget::updateTitle(const char* txt) {
    if (strcmp(txt, "Disabled") == 0 || strcmp(txt, "---") == 0)
        disabled = true;
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
    int area_x = x_offset;
    int area_y = y_offset;
    int area_w = 128;
    int area_h = 44;
    display.fillRect(area_x, area_y, area_w, area_h, 0);

    drawFader();
    updateButtonName(false);
    drawSeparators();

    display.displayBlocking();
}
