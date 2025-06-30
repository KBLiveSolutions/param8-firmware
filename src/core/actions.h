#pragma once
#include <Arduino.h>

void onButtonShortPress(uint8_t idx);
void onButtonLongPress(uint8_t idx);
void onEncoderChange(uint8_t idx);
void onEncoderChange(uint8_t idx, int value);
void updateFaderTitles();
void sendPresetSysEx(uint8_t preset);