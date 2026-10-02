#pragma once

#include <Arduino.h>
#include "sequencer.h" // for SEQ_TRACKS

enum LfoWaveform : uint8_t {
    LFO_SINE = 0,
    LFO_TRIANGLE,
    LFO_SQUARE,
    LFO_SAW,
    LFO_SAMPLE_HOLD,
    LFO_WAVEFORM_COUNT
};

struct LfoTrack {
    LfoWaveform waveform = LFO_SINE;
    float freq = 1.0f;   // Hz
    uint8_t value = 64;  // center value, 0-127
    int8_t  amount = 0;  // modulation depth, clamped so output stays 0-127
    bool armed = false;
    uint8_t ccNumber = 1;
    uint8_t channel = 0; // 0-indexed (displayed as channel+1)
};

// Free-running LFO: phase accumulates from millis() delta, no clock sync.
class Lfo {
public:
    void setup();

    // MIDI realtime clock — no-ops, LFO runs freely regardless of transport.
    void onClockTick() {}
    void onClockStart() {}
    void onClockContinue() {}
    void onClockStop() {}
    bool isRunning() const { return true; }

    // Call from the main loop. Advances phase by freq*dt and sends CC on change.
    void onTimer();

    void arm(uint8_t track, bool on);
    bool isArmed(uint8_t track) const;

    void setWaveform(uint8_t track, LfoWaveform wf);
    LfoWaveform getWaveform(uint8_t track) const;
    void setFreq(uint8_t track, float hz);
    float getFreq(uint8_t track) const;
    void setValue(uint8_t track, uint8_t value);
    uint8_t getValue(uint8_t track) const;
    void setAmount(uint8_t track, int8_t amount);
    int8_t getAmount(uint8_t track) const;
    void setOutput(uint8_t track, uint8_t ccNumber, uint8_t channel);
    uint8_t getCCNumber(uint8_t track) const;
    uint8_t getCCChannel(uint8_t track) const;

    uint8_t getCurrentOutput(uint8_t track) const;
    float getPhase(uint8_t track) const;

    // Value at an arbitrary phase without touching state (used to draw waveform).
    uint8_t previewOutput(uint8_t track, float phase) const;

    static const char* waveformLabel(LfoWaveform wf);

private:
    LfoTrack _tracks[SEQ_TRACKS];
    uint8_t  _lastOutput[SEQ_TRACKS] = {0};
    float    _phase[SEQ_TRACKS] = {};
    float    _shValue[SEQ_TRACKS] = {};  // S&H held value, -1..1
    uint32_t _lastMs = 0;
    uint32_t _lastSendMs[SEQ_TRACKS] = {};
    uint8_t  _sendCursor = 0; // round-robin: next track to check for sending

    static float waveformValue(LfoWaveform wf, float phase);
};

extern Lfo lfo;
