#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "../view/display.h"
#include "../view/leds.h"


void onButtonShortPress(uint8_t idx) {   
    uint8_t channel = controls.getButtonShort(idx).channel;
    ControlMidiType type = controls.getButtonShort(idx).type;
    uint8_t number = controls.getButtonShort(idx).number;
    uint8_t value = controls.getButtonShort(idx).value;
    if (controls.getPreset() == 7) {
        sendMidiMessage( type, number, 127, channel);
        sendMidiMessage( type, number, 0, channel);
    } 
    else {
        value = value < 63 ? 127 : 0; // Convert to MIDI value
        sendMidiMessage( type, number, value, channel);
    }
}

void onButtonLongPress(uint8_t idx) {
    blinkLedBlue(idx, 2);
    char buf[24];
    snprintf(buf, sizeof(buf), "Preset %d", idx + 1);
    buf[sizeof(buf)-1] = '\0';  
    updateDisplayBox("left", buf);
    updateDisplayBox("right", buf);
    // Faire clignoter la LED correspondante 2 fois en bleu
    
    controls.setPreset(idx);
    updateFaderTitles();
    sendPresetSysEx(idx);

    uint8_t channel = controls.getButtonLong(idx).channel;
    ControlMidiType type = controls.getButtonLong(idx).type;
    uint8_t number = controls.getButtonLong(idx).number;
    sendMidiMessage(type, number, 127, channel);
    sendMidiMessage(type, number, 0, channel);

}

void onEncoderChange(uint8_t idx, int value) {
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;
    controls.getEncoder(idx).value = value; // Update the value in the control
    sendMidiMessage(type, number, value, channel);

    updateFader(idx, value);
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