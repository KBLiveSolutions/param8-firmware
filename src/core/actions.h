#pragma once
#include <Arduino.h>

extern bool shiftPressed;
extern bool latchPressed;
extern bool revertMode;

void onButtonShortPress(uint8_t idx);
void onRelativeEncoderChange(uint8_t idx, int value);
void updateFaderTitles();
void sendPresetSysEx(uint8_t preset);
void onShiftPress();
void onShiftRelease();
void onLatchPress();
void onLatchRelease();
void releaseLatchAndSend();
void sendRevertEvents();
void setRevertModeLed(bool on);
void onButtonPressedReleased(uint8_t, bool);
void onButtonRelease(uint8_t idx); 