#include <Arduino.h>
#include <U8g2lib.h>

enum FaderLayout {
    LAYOUT_DYNAMIC,
    LAYOUT_COMPACT,
    LAYOUT_STACKED
};

extern FaderLayout faderLayout;

class FaderWidget {
public:
    static const int BAR_W = 118;
    static const int BAR_H = 8;

    U8G2 &u8g2;
    char title[20];
    char paramName[20];
    char buttonName[20];
    char buttonText[20];
    int value;
    int oldValue;
    int x_offset, y_offset;
    int boutonNumber;

    FaderWidget(U8G2 &u8g2, const char* title, int x, int y, int boutonNumber);

    void setTitle(const char* txt);
    void setValue(int val);
    void setEmpty();
    void draw();
    void drawFader();
    void drawFaderDynamic();
    void drawFaderCompact();
    void drawFaderStacked();
    void drawTitle();
    void setButtonName(const char* txt);
    void drawButtonName(const char* txt, bool);
    void updateButtonName(bool);
    void updateTitle(const char* txt);
    void setParamName(const char* txt);
    void showParamName();
};