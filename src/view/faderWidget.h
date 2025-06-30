#include <Arduino.h>
#include <U8g2lib.h>

class FaderWidget {
public:
    static const int BAR_W = 118;
    static const int BAR_H = 8;

    U8G2 &u8g2;
    char title[20];
    char paramName[20];
    int value;
    int oldValue;
    int x_offset, y_offset;

    // Déclaration seulement !
    FaderWidget(U8G2 &u8g2, const char* title, int x, int y);

    void setTitle(const char* txt);
    void setValue(int val);
    void setEmpty();
    void draw();
    void drawFader();
    void drawTitle();
    void updateTitle(const char* txt);
    void setParamName(const char* txt);
    void showParamName();
};