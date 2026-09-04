#pragma once
#include <Arduino.h>

extern bool shiftPressed;
extern bool latchPressed;
extern bool latchHeld;
extern bool revertMode;
extern unsigned long lastInputTime;
extern unsigned long lastButtonReleaseTime[8];

void onButtonShortPress(uint8_t idx);
void onRelativeEncoderChange(uint8_t idx, int value);
void onAbsoluteEncoderChange(uint8_t idx, int newPos);
void updateFaderTitles();
void updateFaderValues();
void sendPresetSysEx(uint8_t preset);
void sendNameRequest(uint8_t idx, uint8_t isButton);
void sendClearNaming(uint8_t idx, uint8_t isButton);
void checkNamingPending();
void checkLatchPending();
void onShiftPress();
void onShiftRelease();
void onLatchPress();
void onLatchRelease();
void releaseLatchAndSend();
void sendRevertEvents();
void setRevertModeLed(bool on);
void onButtonPressed(uint8_t);
void onButtonReleased(uint8_t);