#include "serial_editor.h"
#include "../midi/midi.h"

bool serialEditorConnected = false;

static uint8_t serialBuf[64];
static size_t serialBufLen = 0;
static bool inSysEx = false;

void serialEditorRead() {
    while (Serial.available()) {
        uint8_t b = Serial.read();

        if (b == 0xF0) {
            inSysEx = true;
            serialBufLen = 0;
            serialBuf[serialBufLen++] = b;
        } else if (inSysEx) {
            if (serialBufLen < sizeof(serialBuf)) {
                serialBuf[serialBufLen++] = b;
            }
            if (b == 0xF7) {
                inSysEx = false;
                serialEditorConnected = true;
                onSysEx(serialBuf, serialBufLen);
            }
        }
    }
}

void serialEditorSend(const uint8_t* data, size_t len) {
    if (serialEditorConnected) {
        Serial.write(data, len);
    }
}
