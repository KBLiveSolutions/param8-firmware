#pragma once

#include <Arduino.h>

enum ControlMidiType : uint8_t {
    MIDI_CC = 0,
    MIDI_NOTE = 1,
    MIDI_PC = 2
};

struct MidiControl {
    ControlMidiType type;
    uint8_t number;
    uint8_t channel;
    uint8_t value = 0; // Valeur réelle synchronisée avec Live
    bool toggleMode = false; // Pour les boutons : mode toggle ou momentané
    unsigned long lastActivity = 0; // Timestamp de la dernière activité
    bool buttonState = false; // Pour les boutons : état actuel (appuyé ou non)
};

struct PresetControls {
    MidiControl encoder[8];
    MidiControl buttons_short[8];
    MidiControl buttons_toggle[8];
    // MidiControl buttons_long[8];
};

class ControlsManager {
public:
    ControlsManager();

    void setPreset(uint8_t preset);
    uint8_t getPreset() const;

    MidiControl& getEncoder(uint8_t idx);
    MidiControl& getButtonShort(uint8_t idx);
   
    void setEncoder(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel);
    void setButtonShort(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel, bool toggleMode);

    void setDefaults();
    void getPresetControls(uint8_t);
    void onControlChange(uint8_t channel, uint8_t control, uint8_t value);
    void checkInactiveEncoders(); // Nouvelle fonction pour vérifier les encodeurs inactifs

private:
    uint8_t _currentPreset;
    PresetControls _presets[8];
};

extern ControlsManager controls;