#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "../view/display.h"
#include "../view/leds.h"

#define MAX_LATCH_EVENTS 256
bool shiftPressed = false; 
bool latchPressed = false;
uint8_t latchEncoderEvents[8][MAX_LATCH_EVENTS] = {{0}};
uint8_t latchEncoderEventCount[8] = {0};
bool revertMode = false;
uint8_t revertEncoderEvents[8][MAX_LATCH_EVENTS] = {{0}};
uint8_t revertEncoderEventCount[8] = {0};

void onShiftPress() {
    shiftPressed = true;
    sendMidiMessage(0, 110, 127, 7);

    if (revertMode) {
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i) {
            revertEncoderEventCount[i] = 0;
        }
    }
}

void onShiftRelease() {
    shiftPressed = false;
    sendMidiMessage(0, 110, 0, 7);
}

void onLatchPress() {
    for (int i = 0; i < 8; ++i) {
        latchEncoderEventCount[i] = 0;
    }
    latchPressed = true;
    sendMidiMessage(0, 111, 127, 7);

    if (revertMode && !shiftPressed) {
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i) {
            revertEncoderEventCount[i] = 0;
        }
        return;
    }

    if (shiftPressed) {
        if (!revertMode) {
            revertMode = true;
            for (int i = 0; i < 8; ++i) {
                revertEncoderEventCount[i] = 0;
            }
            setRevertModeLed(true);
        } else {
            sendRevertEvents();
            revertMode = false;
            setRevertModeLed(false);
            for (int i = 0; i < 8; ++i) {
                revertEncoderEventCount[i] = 0;
            }
        }
    }
}

void onLatchRelease() {
    latchPressed = false;
    releaseLatchAndSend();
    sendMidiMessage(0, 111, 0, 7);
}

void onButtonShortPress(uint8_t idx) {   
}


void onButtonPressedReleased(uint8_t idx, bool pressed) {
    int _value = pressed ? 127 : 0;
    if (shiftPressed) {
        blinkLedBlue(idx, 2);
        char buf[24];
        snprintf(buf, sizeof(buf), "Preset %d", idx + 1);
        buf[sizeof(buf)-1] = '\0';  
        updateDisplayBox("left", buf);
        updateDisplayBox("right", buf);

        controls.setPreset(idx);
        updateFaderTitles();
        sendPresetSysEx(idx);

        ControlMidiType type_long = controls.getButtonLong(idx).type;
        uint8_t number_long = controls.getButtonLong(idx).number;
        uint8_t channel_long = controls.getButtonLong(idx).channel;
        sendMidiMessage(type_long, number_long, _value, channel_long);
        return;
    }

    else
    {
        uint8_t channel = controls.getButtonShort(idx).channel;
        ControlMidiType type = controls.getButtonShort(idx).type;
        uint8_t number = controls.getButtonShort(idx).number;
        // uint8_t value = controls.getButtonShort(idx).value;
        sendMidiMessage(type, number, _value, channel);
    }
}

void onButtonRelease(uint8_t idx) {
  // Gérer l'événement de relâchement du bouton ici
}

void onRelativeEncoderChange(uint8_t idx, int delta) {
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;

    controls.getEncoder(idx).lastActivity = millis();

    uint8_t relValue = (uint8_t)(delta & 0x7F);

    // Calculer et mettre à jour la valeur cumulative
    int estimated_display = controls.getEncoder(idx).value + delta;
    if (estimated_display < 0) estimated_display = 0;
    if (estimated_display > 127) estimated_display = 127;
    
    // Mettre à jour la valeur stockée dans le contrôle
    controls.getEncoder(idx).value = estimated_display;

    if (revertMode) {
        if (revertEncoderEventCount[idx] < MAX_LATCH_EVENTS) {
            revertEncoderEvents[idx][revertEncoderEventCount[idx]++] = relValue;
        }
        sendMidiMessage(type, number, relValue, channel);
    } else if (!latchPressed) {
        sendMidiMessage(type, number, relValue, channel);
    } else {
        if (latchEncoderEventCount[idx] < MAX_LATCH_EVENTS) {
            latchEncoderEvents[idx][latchEncoderEventCount[idx]++] = relValue;
        }
    }
    
    // Mettre à jour le fader avec la valeur cumulative
    // if (latchPressed || controls.getPreset() < 6) updateFader(idx, (uint8_t)estimated_display);
}

void updateFaderTitles() {
    for(int i = 0; i < 8; ++i) {
        char buf[24];
        int number = controls.getEncoder(i).number;
        int channel = controls.getEncoder(i).channel;
        int preset = controls.getPreset();
        snprintf(buf, sizeof(buf), "CC%d / %d", number, preset + 1);
        faders[i]->setParamName(buf);
    }
}

void sendPresetSysEx(uint8_t preset) {
    uint8_t packet[5] = { 240, 111, 4, preset, 247 };
    usb_midi.writePacket(packet);
    usb_midi.write(packet, 5);
}

void releaseLatchAndSend() {
    // Trouver le nombre maximum d'événements parmi tous les encodeurs
    uint8_t maxEvents = 0;
    for (int i = 0; i < 8; ++i) {
        if (latchEncoderEventCount[i] > maxEvents) {
            maxEvents = latchEncoderEventCount[i];
        }
    }
    
    // Envoyer les événements de manière entrelacée
    for (uint8_t eventIndex = 0; eventIndex < maxEvents; ++eventIndex) {
        for (int i = 0; i < 8; ++i) {
            // Vérifier s'il y a encore des événements pour cet encodeur
            if (eventIndex < latchEncoderEventCount[i]) {
                uint8_t channel = controls.getEncoder(i).channel;
                ControlMidiType type = controls.getEncoder(i).type;
                uint8_t number = controls.getEncoder(i).number;
                
                sendMidiMessage(type, number, latchEncoderEvents[i][eventIndex], channel);
                delay(1); // Délai plus court car on alterne entre les encodeurs
            }
        }
    }
    
    // Réinitialiser les compteurs
    for (int i = 0; i < 8; ++i) {
        latchEncoderEventCount[i] = 0;
    }
}

void sendRevertEvents() {
    for (int i = 0; i < 8; ++i) {
        uint8_t channel = controls.getEncoder(i).channel;
        ControlMidiType type = controls.getEncoder(i).type;
        uint8_t number = controls.getEncoder(i).number;
        for (uint8_t j = 0; j < revertEncoderEventCount[i]; ++j) {
            uint8_t val = revertEncoderEvents[i][j];
            uint8_t inv = 0;
            if (val == 0) inv = 0;
            else if (val <= 0x3F) inv = (0x80 - val) & 0x7F;
            else inv = (0x80 - val) & 0x7F;
            sendMidiMessage(type, number, inv, channel);
        }
    }
}

void setRevertModeLed(bool on) {
    if (!on) {
        showLed(8, 0, 0, 0);
    }
}