#pragma once

#include <Arduino.h>

#define SEQ_STEPS 16
#define SEQ_TRACKS 8

enum SeqRate : uint8_t {
    SEQ_RATE_1_4 = 0,
    SEQ_RATE_1_8,
    SEQ_RATE_1_8T,
    SEQ_RATE_1_16,
    SEQ_RATE_1_16T,
    SEQ_RATE_1_32,
    SEQ_RATE_COUNT
};

struct StepSequence {
    uint8_t steps[SEQ_STEPS] = {0};
    SeqRate rate = SEQ_RATE_1_16;
    bool armed = false;
};

// Drives up to 8 independent 16-step sequences (one per encoder/fader),
// advanced by an incoming MIDI clock (24 pulses per quarter note).
class Sequencer {
public:
    void setup();

    // MIDI realtime handlers, call from the incoming-clock parser.
    void onClockTick();
    void onClockStart();
    void onClockStop();
    bool isRunning() const { return _running; }

    // Per-track (encoder index 0-7) control.
    void arm(uint8_t track, bool on);
    bool isArmed(uint8_t track) const;
    void setStepValue(uint8_t track, uint8_t step, uint8_t value);
    uint8_t getStepValue(uint8_t track, uint8_t step) const;
    void setRate(uint8_t track, SeqRate rate);
    SeqRate getRate(uint8_t track) const;
    uint8_t getCurrentStep(uint8_t track) const;

    static uint8_t ticksPerStep(SeqRate rate);
    static const char* rateLabel(SeqRate rate);

private:
    StepSequence _sequences[SEQ_TRACKS];
    uint8_t _stepIndex[SEQ_TRACKS] = {0};
    uint32_t _tickCount = 0;
    bool _running = false;

    void advanceStep(uint8_t track);
};

extern Sequencer sequencer;
