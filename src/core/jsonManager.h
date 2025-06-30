#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>

struct ControlData {
    int value0;
    int value1;
    int value2;
};

class JsonManager {
public:
    JsonManager(const char* path = "data/data.json");

    bool load();
    bool save();

    int getMode();
    void setMode(int mode);

    JsonObject getPreset(uint8_t idx);

    JsonArray getButtonShort(uint8_t preset, uint8_t btn);
    void setButtonShort(uint8_t preset, uint8_t btn, int v0, int v1);

    JsonArray getButtonLong(uint8_t preset, uint8_t btn);
    void setButtonLong(uint8_t preset, uint8_t btn, int v0, int v1);

    JsonArray getEncoder(uint8_t preset, uint8_t enc);
    void getEncoderArray(uint8_t preset, uint8_t enc, int result[2]);
    void setEncoder(uint8_t preset, uint8_t enc, int v0, int v1);
    ControlData getControlData(const char* control_type, uint8_t preset, uint8_t enc);

private:
    const char* _path;
    JsonDocument _doc;
};
void setupJsonManager();
extern JsonManager json;