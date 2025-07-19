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
    if (controls.getPreset() > 5) {
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

void onEncoderChange(uint8_t idx, int delta) {
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;
    
    // Mettre à jour le timestamp d'activité
    controls.getEncoder(idx).lastActivity = millis();
    
    int _value; // Utiliser int pour permettre les valeurs négatives temporaires
    
    if(controls.getPreset() > 5) {
        // Mode relative: 64 = centre, >64 = increment, <64 = decrement
        _value = 64 + delta;
        // Clamp entre 1 et 127 pour le mode relatif
        if (_value < 1) _value = 1;
        if (_value > 127) _value = 127;
        
        sendMidiMessage(type, number, (uint8_t)_value, channel);
        
        // En mode relatif: afficher une estimation locale mais NE PAS modifier value
        int estimated_display = controls.getEncoder(idx).value + delta;
        if (estimated_display < 0) estimated_display = 0;
        if (estimated_display > 127) estimated_display = 127;
        // updateFader(idx, (uint8_t)estimated_display);
        
    } else {
        // Mode absolu: mettre à jour la valeur stockée normalement
        controls.getEncoder(idx).value += delta;
        _value = controls.getEncoder(idx).value;
        
        // Clamp entre 0 et 127 pour le mode absolu
        if (_value < 0) {
            _value = 0;
            controls.getEncoder(idx).value = 0;
        }
        if (_value > 127) {
            _value = 127;
            controls.getEncoder(idx).value = 127;
        }
        
        sendMidiMessage(type, number, (uint8_t)_value, channel);
        updateFader(idx, (uint8_t)_value);
    }
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