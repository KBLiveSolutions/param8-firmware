#pragma once
#include <Arduino.h>

extern bool serialEditorConnected;

void serialEditorRead();
void serialEditorSend(const uint8_t* data, size_t len);
