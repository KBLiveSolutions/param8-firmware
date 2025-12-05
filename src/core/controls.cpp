#include "controls.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"
#include "../input/encoders.h"
#include "../midi/midi.h"

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

MidiControl& ControlsManager::getButtonLong(uint8_t idx) {
    return _presets[_currentPreset].buttons_long[idx];
}

void ControlsManager::setEncoder(uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (idx < 8) {
        _presets[_currentPreset].encoder[idx] = {type, number, channel};
    }
}

void ControlsManager::setButtonShort(uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (idx < 8) {
        _presets[_currentPreset].buttons_short[idx] = {type, number, channel};
    }
}

void ControlsManager::setButtonLong(uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (idx < 8) {
        _presets[_currentPreset].buttons_long[idx] = {type, number, channel};
    }
}

// Surcharges avec paramètre de preset
void ControlsManager::setEncoder(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (preset < 8 && idx < 8) {
        _presets[preset].encoder[idx] = {type, number, channel};
    }
}

void ControlsManager::setButtonShort(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (preset < 8 && idx < 8) {
        _presets[preset].buttons_short[idx] = {type, number, channel};
    }
}

void ControlsManager::setButtonLong(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel) {
    if (preset < 8 && idx < 8) {
        _presets[preset].buttons_long[idx] = {type, number, channel};
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
            ControlData data = json.getControlData("encoders", _preset, i);
            
            Serial.print("  Encoder ");
            Serial.print(i);
            Serial.print(" - Raw data: type=");
            Serial.print(data.value0);  // Devrait être 0 (CC)
            Serial.print(", number=");
            Serial.print(data.value1);  // Devrait être 10, 11, 12, etc.
            Serial.print(", channel=");
            Serial.println(data.value2); // Devrait être 0
            
            if (data.value1 == 0) {
                Serial.println("    ❌ PROBLÈME: CC number = 0 au lieu de la vraie valeur!");
            }
            
            ControlMidiType type = static_cast<ControlMidiType>(data.value0);
            uint8_t number = static_cast<uint8_t>(data.value1);
            uint8_t channel = static_cast<uint8_t>(data.value2);
            
            setEncoder(_preset, i, type, number, channel);
            _presets[_preset].encoder[i].value = 64;
            _presets[_preset].encoder[i].lastActivity = 0;

            // Charger la configuration des boutons courts
            data = json.getControlData("buttons_short", _preset, i);
            type = static_cast<ControlMidiType>(data.value0);
            number = static_cast<uint8_t>(data.value1);
            channel = static_cast<uint8_t>(data.value2);
            setButtonShort(_preset, i, type, number, channel);
            
            // AJOUTER : Initialiser les valeurs des boutons
            _presets[_preset].buttons_short[i].value = 0;
            
            // Charger la configuration des boutons longs  
            data = json.getControlData("buttons_long", _preset, i);
            type = static_cast<ControlMidiType>(data.value0);
            number = static_cast<uint8_t>(data.value1);
            channel = static_cast<uint8_t>(data.value2);
            setButtonLong(_preset, i, type, number, channel);
            
            // AJOUTER : Initialiser les valeurs des boutons
            _presets[_preset].buttons_long[i].value = 0;
        }
    }
    
    // Synchroniser les positions des encodeurs avec le preset actuel
    for (int i = 0; i < 8; i++) {
        encoders.positions[i] = _presets[_currentPreset].encoder[i].value;
    }
}

void ControlsManager::onControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    Serial.print("Control Change - Channel: ");
    Serial.print(channel);
    Serial.print(", Control: ");
    Serial.print(control);
    Serial.print(", Value: ");
    Serial.println(value);
    
    for (int i = 0; i < 8; ++i) {
        if (getEncoder(i).channel == channel && getEncoder(i).number == control) {
            // if(getEncoder(i).value == value) faders[i]->showParamName();
            faders[i]->setValue(value);
            getEncoder(i).value = value;
            getEncoder(i).lastActivity = millis(); // Mettre à jour le timestamp d'activité
            encoders.positions[i] = value; 
        }
        // Serial.print("Short Button:     ");
        // Serial.print(i); 
        // Serial.print(" - Channel: ");
        // Serial.print(getButtonShort(i).channel);
        // Serial.print(", Control: ");
        // Serial.print(getButtonShort(i).number);
        if (getButtonShort(i).channel == channel && getButtonShort(i).number == control) {
            if(value > 63) showLed(i, 255, 255, 255); 
            else showLed(i, 0, 0, 0); // Éteint la LED si la valeur est inférieure ou égale à 63
            getButtonShort(i).value = value;
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