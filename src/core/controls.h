#pragma once

#include <Arduino.h>

enum ControlMidiType : uint8_t {
    MIDI_CC = 0,
    MIDI_NOTE = 1,
    MIDI_PC = 2,
    MIDI_AT = 3,  // Channel Aftertouch
    MIDI_PB = 4   // Pitch Bend (14-bit)
};

struct MidiControl {
    ControlMidiType type;
    uint8_t number;
    uint8_t channel;
    uint8_t value = 0;
    bool toggleMode = false;
    unsigned long lastActivity = 0;
    char controlName[12] = {0};
    bool hasWatcher = false;
    bool hiRes = false;
    uint8_t minVal = 0;
    uint8_t maxVal = 127;
};

struct PresetControls {
    MidiControl encoder[8];
    MidiControl buttons_short[8];
    MidiControl buttons_toggle[8];
    char presetName[20] = {0};
};

class ControlsManager {
public:
    ControlsManager();

    void setPreset(uint8_t preset);
    uint8_t getPreset() const;

    MidiControl& getEncoder(uint8_t idx);
    MidiControl& getButtonShort(uint8_t idx);
    MidiControl& getEncoderAt(uint8_t preset, uint8_t idx);
    MidiControl& getButtonShortAt(uint8_t preset, uint8_t idx);
   
    void setEncoder(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel);
    void setButtonShort(uint8_t preset, uint8_t idx, ControlMidiType type, uint8_t number, uint8_t channel, bool toggleMode);

    const char* getPresetName(uint8_t preset);
    void setPresetName(uint8_t preset, const char* name);
    void setDefaults();
    void getPresetControls(uint8_t);
    void onMidiValueChange(uint8_t channel, uint8_t control, uint8_t value);
    void checkInactiveEncoders();

    uint16_t getHiResValue(uint8_t preset, uint8_t idx) const { return _hiResValues[preset][idx]; }
    void setHiResValue(uint8_t preset, uint8_t idx, uint16_t v) { _hiResValues[preset][idx] = v; }

private:
    uint8_t _currentPreset;
    PresetControls _presets[26];
    uint16_t _hiResValues[26][8];
};

extern ControlsManager controls;