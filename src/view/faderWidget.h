#include <Arduino.h>
#include <PicoGFX_SSD1322.h>

enum FaderLayout {
    LAYOUT_DYNAMIC,
    LAYOUT_COMPACT
};

extern FaderLayout faderLayout;

class FaderWidget {
public:
    static const int BAR_W = 118;
    static const int BAR_H = 8;

    PicoGFX_SSD1322 &display;
    char title[20];
    char paramName[20];
    char buttonName[20];
    char buttonText[20];
    int value;
    int oldValue;
    bool showingValue = false;
    bool valueOnly = false;
    bool disabled = false;
    bool dimmed = false;
    bool buttonState = false;
    int x_offset, y_offset;
    int boutonNumber;

    FaderWidget(PicoGFX_SSD1322 &display, const char* title, int x, int y, int boutonNumber);

    void setTitle(const char* txt);
    void setValue(int val);
    void setEmpty();
    void draw();
    void drawFader();
    void drawFaderDynamic();
    void drawFaderCompact();
    void drawTitle();
    void setButtonName(const char* txt);
    void drawButtonName(const char* txt, bool);
    void updateButtonName(bool);
    void updateTitle(const char* txt);
    void setParamName(const char* txt);
    void showParamName();
    void drawSeparators();
};
