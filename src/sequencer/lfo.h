#pragma once

#include <Arduino.h>
#include "sequencer.h" // reuse SeqRate + SEQ_TRACKS

enum LfoWaveform : uint8_t {
    LFO_SINE = 0,
    LFO_TRIANGLE,
    LFO_SQUARE,
    LFO_SAW_UP,
    LFO_SAW_DOWN,
    LFO_WAVEFORM_COUNT
};

struct LfoTrack {
    LfoWaveform waveform = LFO_SINE;
    SeqRate rate = SEQ_RATE_1_4; // cycle length, synced to clock (same rate table as Sequencer)
    uint8_t value = 64;          // center value, 0-127 - same meaning as the CC value, the LFO oscillates around it
    uint8_t amount = 32;         // modulation depth, 0-127
    bool armed = false;
    uint8_t ccNumber = 1;
    uint8_t channel = 0; // 0-indexed (displayed as channel+1)
};

// Clock-synced per-encoder LFO: continuously modulates a CC value around a
// center point. One independent LFO per encoder (0-7), same track indexing
// as Sequencer so the two can share an encoder (step-seq XOR LFO per track).
class Lfo {
public:
    void setup();

    // MIDI realtime handlers, call from the incoming-clock parser (same
    // clock as Sequencer).
    void onClockTick();
    void onClockStart();
    void onClockStop();
    bool isRunning() const { return _running; }

    void arm(uint8_t track, bool on);
    bool isArmed(uint8_t track) const;

    void setWaveform(uint8_t track, LfoWaveform wf);
    LfoWaveform getWaveform(uint8_t track) const;
    void setRate(uint8_t track, SeqRate rate);
    SeqRate getRate(uint8_t track) const;
    void setValue(uint8_t track, uint8_t value);
    uint8_t getValue(uint8_t track) const;
    void setAmount(uint8_t track, uint8_t amount);
    uint8_t getAmount(uint8_t track) const;
    void setOutput(uint8_t track, uint8_t ccNumber, uint8_t channel);

    uint8_t getCurrentOutput(uint8_t track) const;
    float getPhase(uint8_t track) const; // 0..1, for display

    // Value the LFO would output at an arbitrary phase (0..1), without
    // touching any state. Used by views to draw the waveform shape.
    uint8_t previewOutput(uint8_t track, float phase) const;

    static const char* waveformLabel(LfoWaveform wf);

private:
    LfoTrack _tracks[SEQ_TRACKS];
    uint8_t _lastOutput[SEQ_TRACKS] = {0};
    uint32_t _tickCount = 0;
    bool _running = false;

    void updateTrack(uint8_t track);
    static float waveformValue(LfoWaveform wf, float phase);
};

extern Lfo lfo;
