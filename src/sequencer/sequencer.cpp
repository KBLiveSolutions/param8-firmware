#include "sequencer.h"
#include "../core/controls.h"
#include "../midi/midi.h"
#include "../view/display.h"

Sequencer sequencer;

void Sequencer::setup()
{
    for (uint8_t i = 0; i < SEQ_TRACKS; i++) {
        _sequences[i] = StepSequence();
        _stepIndex[i] = 0;
    }
    _tickCount = 0;
    _running = false;
}

uint8_t Sequencer::ticksPerStep(SeqRate rate)
{
    // 24 pulses-per-quarter-note MIDI clock.
    switch (rate) {
        case SEQ_RATE_1_4:   return 24;
        case SEQ_RATE_1_8:   return 12;
        case SEQ_RATE_1_8T:  return 8;
        case SEQ_RATE_1_16:  return 6;
        case SEQ_RATE_1_16T: return 4;
        case SEQ_RATE_1_32:  return 3;
        default:             return 6;
    }
}

const char* Sequencer::rateLabel(SeqRate rate)
{
    switch (rate) {
        case SEQ_RATE_1_4:   return "1/4";
        case SEQ_RATE_1_8:   return "1/8";
        case SEQ_RATE_1_8T:  return "1/8T";
        case SEQ_RATE_1_16:  return "1/16";
        case SEQ_RATE_1_16T: return "1/16T";
        case SEQ_RATE_1_32:  return "1/32";
        default:             return "?";
    }
}

void Sequencer::onClockStart()
{
    _tickCount = 0;
    _running = true;
    for (uint8_t i = 0; i < SEQ_TRACKS; i++)
        _stepIndex[i] = 0;
}

void Sequencer::onClockStop()
{
    _running = false;
}

void Sequencer::onClockTick()
{
    if (!_running) return;
    _tickCount++;
    for (uint8_t i = 0; i < SEQ_TRACKS; i++) {
        if (!_sequences[i].armed) continue;
        uint8_t tps = ticksPerStep(_sequences[i].rate);
        if (_tickCount % tps == 0)
            advanceStep(i);
    }
}

void Sequencer::advanceStep(uint8_t track)
{
    _stepIndex[track] = (_stepIndex[track] + 1) % SEQ_STEPS;
    uint8_t value = _sequences[track].steps[_stepIndex[track]];

    MidiControl& enc = controls.getEncoder(track);
    sendMidiMessage(enc.type, enc.number, value, enc.channel);
    enc.value = value;
    updateFader(track, value);
}

void Sequencer::arm(uint8_t track, bool on)
{
    if (track >= SEQ_TRACKS) return;
    _sequences[track].armed = on;
    if (on) _stepIndex[track] = 0;
}

bool Sequencer::isArmed(uint8_t track) const
{
    return track < SEQ_TRACKS && _sequences[track].armed;
}

void Sequencer::setStepValue(uint8_t track, uint8_t step, uint8_t value)
{
    if (track >= SEQ_TRACKS || step >= SEQ_STEPS) return;
    _sequences[track].steps[step] = value;
}

uint8_t Sequencer::getStepValue(uint8_t track, uint8_t step) const
{
    if (track >= SEQ_TRACKS || step >= SEQ_STEPS) return 0;
    return _sequences[track].steps[step];
}

void Sequencer::setRate(uint8_t track, SeqRate rate)
{
    if (track >= SEQ_TRACKS) return;
    _sequences[track].rate = rate;
}

SeqRate Sequencer::getRate(uint8_t track) const
{
    return track < SEQ_TRACKS ? _sequences[track].rate : SEQ_RATE_1_16;
}

uint8_t Sequencer::getCurrentStep(uint8_t track) const
{
    return track < SEQ_TRACKS ? _stepIndex[track] : 0;
}
