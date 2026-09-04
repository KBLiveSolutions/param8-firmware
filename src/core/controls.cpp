#include "controls.h"
#include "actions.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"
#include "../input/encoders.h"
#include "../midi/midi.h"
#include "../usb/serial_editor.h"

ControlsManager controls;

ControlsManager::ControlsManager() : _currentPreset(0) {
    // Initialisation par défaut (tout à zéro)
    memset(_presets, 0, sizeof(_presets));
}

void ControlsManager::setPreset(uint8_t preset) {
    if (preset < 8) {
        _currentPreset = preset;
        
        // Charger les données du nouveau preset depuis le JSON
        json.setMode(preset);
        
        // Synchroniser les positions des encodeurs avec les nouvelles valeurs
        for (int i = 0; i < 8; i++) {
            encoders.positions[i] = _presets[_currentPreset].encoder[i].value;
        }
        
        // Sauvegarder le changement de preset
        json.save();
    }
}

uint8_t ControlsManager::getPreset() const {
    return _currentPreset;
}

MidiControl& ControlsManager::getEncoder(uint8_t idx) {
    return _presets[_currentPreset].encoder[idx];
}

MidiControl& ControlsManager::getButtonShort(uint8_t idx) {
    return _presets[_currentPreset].buttons_short[idx];
}

MidiControl& ControlsManager::getEncoderAt(uint8_t preset, uint8_t idx) {
    return _presets[preset].encoder[idx];
}

MidiControl& ControlsManager::getButtonShortAt(uint8_t preset, uint8_t idx) {
    return _presets[preset].buttons_short[idx];
}

// Surcharges avec paramètre de preset
void ControlsManager::setEncoder(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (preset < 8 && idx < 8) {
        _presets[preset].encoder[idx] = {type, number, channel};
        json.setEncoder(preset, idx, static_cast<int>(type), static_cast<int>(number), static_cast<int>(channel)); 
    }
}

void ControlsManager::setButtonShort(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel, bool toggleMode) {
    if (preset < 8 && idx < 8) {
        _presets[preset].buttons_short[idx] = {type, number, channel};
        _presets[preset].buttons_short[idx].toggleMode = toggleMode;
    }
}

const char* ControlsManager::getPresetName(uint8_t preset) {
    if (preset < 8) return _presets[preset].presetName;
    return "";
}

void ControlsManager::setPresetName(uint8_t preset, const char* name) {
    if (preset < 8) {
        strncpy(_presets[preset].presetName, name, sizeof(_presets[preset].presetName) - 1);
        _presets[preset].presetName[sizeof(_presets[preset].presetName) - 1] = '\0';
    }
}

void ControlsManager::setDefaults() {
    _currentPreset = json.getMode();
    Serial.print("Preset actuel chargé: ");
    Serial.println(_currentPreset);
    
    for(int _preset = 0; _preset < 8; ++_preset){
        Serial.print("Chargement preset ");
        Serial.println(_preset);

        for(int i = 0; i < 8; ++i) {
            if (_preset == 6) {
                // Mixer/Global: encoders CC 40-47, buttons CC 50-57, all on channel 7
                setEncoder(_preset, i, MIDI_CC, 40 + i, 7);
                setButtonShort(_preset, i, MIDI_CC, 50 + i, 7, false);
            } else if (_preset == 7) {
                // Device: encoders CC 10-17, buttons CC 20-27, all on channel 7
                setEncoder(_preset, i, MIDI_CC, 10 + i, 7);
                setButtonShort(_preset, i, MIDI_CC, 20 + i, 7, false);
            } else {
                ControlData data = json.getControlData("encoders", _preset, i);
                ControlMidiType type = static_cast<ControlMidiType>(data.value0);
                uint8_t number = static_cast<uint8_t>(data.value1);
                uint8_t channel = static_cast<uint8_t>(data.value2);
                setEncoder(_preset, i, type, number, channel);

                data = json.getControlData("buttons_short", _preset, i);
                type = static_cast<ControlMidiType>(data.value0);
                number = static_cast<uint8_t>(data.value1);
                channel = static_cast<uint8_t>(data.value2);
                bool toggleMode = json.getButtonToggleMode(_preset, i) > 0;
                setButtonShort(_preset, i, type, number, channel, toggleMode);

                const char* encName = json.getDoc()[String(_preset)]["encoder_names"][String(i)] | "";
                strncpy(_presets[_preset].encoder[i].controlName, encName, 11);
                const char* btnName = json.getDoc()[String(_preset)]["button_names"][String(i)] | "";
                strncpy(_presets[_preset].buttons_short[i].controlName, btnName, 11);
            }

            _presets[_preset].encoder[i].value = 64;
            _presets[_preset].encoder[i].lastActivity = 0;
            _presets[_preset].buttons_short[i].value = 0;
        }
        if (_preset < 6) {
            const char* pName = json.getDoc()[String(_preset)]["preset_name"] | "";
            strncpy(_presets[_preset].presetName, pName, sizeof(_presets[_preset].presetName) - 1);
        }
    }

    // Synchroniser les positions des encodeurs avec le preset actuel
    for (int i = 0; i < 8; i++) {
        encoders.positions[i] = _presets[_currentPreset].encoder[i].value;
    }
}

void ControlsManager::onMidiValueChange(uint8_t channel, uint8_t control, uint8_t value) {
    Serial.print("Control Change - Channel: ");
    Serial.print(channel);
    Serial.print(", Control: ");
    Serial.print(control);
    Serial.print(", Value: ");
    Serial.println(value);
    
    for (int i = 0; i < 8; ++i) {
        if (getEncoder(i).channel == channel && getEncoder(i).number == control) {
            getEncoder(i).value = value;
            faders[i]->setValue(value);
            encoders.positions[i] = value;
            if (!getEncoder(i).hasWatcher && _currentPreset < 6) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", value);
                faders[i]->updateTitle(buf);
            }
        }
        if (getButtonShort(i).channel == channel && getButtonShort(i).number == control) {
            getButtonShort(i).value = value;
            bool on = (getButtonShort(i).type == MIDI_NOTE) ? (value == 127) : (value > 63);
            faders[i]->updateButtonName(on);
        }
    }
}

void ControlsManager::checkInactiveEncoders() {
    unsigned long currentTime = millis();
    const unsigned long INACTIVITY_TIMEOUT = 500; // 1 seconde en millisecondes
    
    for (int i = 0; i < 8; ++i) {
        // Vérifier si l'encodeur a été inactif pendant plus d'1 seconde
        if (getEncoder(i).lastActivity != 0 && 
            (currentTime - getEncoder(i).lastActivity) > INACTIVITY_TIMEOUT) {
            
            // Afficher le nom du paramètre
            faders[i]->showParamName();
            
            // Réinitialiser le timestamp pour éviter l'appel répétitif
            getEncoder(i).lastActivity = 0;
        }
    }
}

void ControlsManager::getPresetControls(uint8_t preset) {
    Serial.print("Envoi des contrôles du preset ");
    Serial.println(preset);

    if (preset < 8) {
        for (int i = 0; i < 8; ++i) {
            MidiControl& encoder = _presets[preset].encoder[i];

            uint8_t packet[9] = { 240, 111, 12, preset, (uint8_t)i,
                                  static_cast<uint8_t>(encoder.type),
                                  encoder.number,
                                  encoder.channel,
                                  247 };
            usb_midi.write(packet, 9);
            serialEditorSend(packet, 9);
            delay(2);

            uint8_t buttonShortPacket[10] = { 240, 111, 13, preset, (uint8_t)i,
                                  static_cast<uint8_t>(_presets[preset].buttons_short[i].type),
                                  _presets[preset].buttons_short[i].number,
                                  _presets[preset].buttons_short[i].channel,
                                  _presets[preset].buttons_short[i].toggleMode ? 1 : 0,
                                  247 };
            usb_midi.write(buttonShortPacket, 10);
            serialEditorSend(buttonShortPacket, 10);
            delay(2);

            if (encoder.controlName[0] != '\0') {
                uint8_t namePacket[20] = { 240, 111, 15, preset, (uint8_t)i, 0 };
                size_t j = 6;
                for (size_t c = 0; c < 11 && encoder.controlName[c] != '\0'; c++) {
                    namePacket[j++] = (uint8_t)encoder.controlName[c];
                }
                namePacket[j++] = 247;
                usb_midi.write(namePacket, j);
                serialEditorSend(namePacket, j);
                delay(2);
            }

            MidiControl& btn = _presets[preset].buttons_short[i];
            if (btn.controlName[0] != '\0') {
                uint8_t namePacket[20] = { 240, 111, 15, preset, (uint8_t)i, 1 };
                size_t j = 6;
                for (size_t c = 0; c < 11 && btn.controlName[c] != '\0'; c++) {
                    namePacket[j++] = (uint8_t)btn.controlName[c];
                }
                namePacket[j++] = 247;
                usb_midi.write(namePacket, j);
                serialEditorSend(namePacket, j);
                delay(2);
            }
        }
        if (_presets[preset].presetName[0] != '\0') {
            uint8_t namePacket[28] = { 240, 111, 0x11, preset };
            size_t j = 4;
            for (size_t c = 0; c < 19 && _presets[preset].presetName[c] != '\0'; c++) {
                namePacket[j++] = (uint8_t)_presets[preset].presetName[c];
            }
            namePacket[j++] = 247;
            usb_midi.write(namePacket, j);
            serialEditorSend(namePacket, j);
            delay(2);
        }

        uint8_t layoutPacket[5] = { 240, 111, 14, (uint8_t)faderLayout, 247 };
        usb_midi.write(layoutPacket, 5);
        serialEditorSend(layoutPacket, 5);

        uint16_t ssSec = (uint16_t)(screenSaverDelay / 1000UL);
        uint8_t ssPacket[6] = { 240, 111, 16, (uint8_t)((ssSec >> 7) & 0x7F), (uint8_t)(ssSec & 0x7F), 247 };
        usb_midi.write(ssPacket, 6);
        serialEditorSend(ssPacket, 6);

        int bright = json.getDoc()["brightness"] | 1;
        uint8_t brPacket[5] = { 240, 111, 0x1B, (uint8_t)(bright & 0x7F), 247 };
        usb_midi.write(brPacket, 5);
        serialEditorSend(brPacket, 5);
    }
}