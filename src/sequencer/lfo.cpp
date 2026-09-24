#include "lfo.h"
#include "../core/controls.h" // for the ControlMidiType/MIDI_CC enum only
#include "../midi/midi.h"
#include <math.h>

Lfo lfo;

void Lfo::setup()
{
    for (uint8_t i = 0; i < SEQ_TRACKS; i++) {
        _tracks[i] = LfoTrack();
        _lastOutput[i] = 0;
    }
    _tickCount = 0;
    _running = false;
}

void Lfo::onClockStart()
{
    _tickCount = 0;
    _running = true;
}

void Lfo::onClockStop()
{
    _running = false;
}

float Lfo::waveformValue(LfoWaveform wf, float phase)
{
    switch (wf) {
        case LFO_SINE:
            return sinf(2.0f * (float)M_PI * phase);
        case LFO_TRIANGLE:
            return 1.0f - 4.0f * fabsf(phase - 0.5f);
        case LFO_SQUARE:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LFO_SAW_UP:
            return 2.0f * phase - 1.0f;
        case LFO_SAW_DOWN:
            return 1.0f - 2.0f * phase;
        default:
            return 0.0f;
    }
}

void Lfo::updateTrack(uint8_t track)
{
    LfoTrack &t = _tracks[track];
    uint32_t ticksPerCycle = Sequencer::ticksPerStep(t.rate);
    if (ticksPerCycle == 0) ticksPerCycle = 1;

    float phase = (float)(_tickCount % ticksPerCycle) / (float)ticksPerCycle;
    uint8_t output = previewOutput(track, phase);

    if (output != _lastOutput[track]) {
        _lastOutput[track] = output;
        sendMidiMessage(MIDI_CC, t.ccNumber, output, t.channel);
    }
}

uint8_t Lfo::previewOutput(uint8_t track, float phase) const
{
    if (track >= SEQ_TRACKS) return 0;
    const LfoTrack &t = _tracks[track];
    float wave = waveformValue(t.waveform, phase);
    int output = (int)t.value + (int)roundf(wave * (float)t.amount);
    return (uint8_t)constrain(output, 0, 127);
}

void Lfo::onClockTick()
{
    if (!_running) return;
    _tickCount++;
    for (uint8_t i = 0; i < SEQ_TRACKS; i++) {
        if (_tracks[i].armed)
            updateTrack(i);
    }
}

void Lfo::arm(uint8_t track, bool on)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].armed = on;
}

bool Lfo::isArmed(uint8_t track) const
{
    return track < SEQ_TRACKS && _tracks[track].armed;
}

void Lfo::setWaveform(uint8_t track, LfoWaveform wf)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].waveform = wf;
}

LfoWaveform Lfo::getWaveform(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].waveform : LFO_SINE;
}

void Lfo::setRate(uint8_t track, SeqRate rate)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].rate = rate;
}

SeqRate Lfo::getRate(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].rate : SEQ_RATE_1_4;
}

void Lfo::setValue(uint8_t track, uint8_t value)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].value = constrain(value, 0, 127);
}

uint8_t Lfo::getValue(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].value : 64;
}

void Lfo::setAmount(uint8_t track, uint8_t amount)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].amount = constrain(amount, 0, 127);
}

uint8_t Lfo::getAmount(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].amount : 0;
}

void Lfo::setOutput(uint8_t track, uint8_t ccNumber, uint8_t channel)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].ccNumber = ccNumber;
    _tracks[track].channel = channel;
}

uint8_t Lfo::getCurrentOutput(uint8_t track) const
{
    return track < SEQ_TRACKS ? _lastOutput[track] : 0;
}

float Lfo::getPhase(uint8_t track) const
{
    if (track >= SEQ_TRACKS) return 0.0f;
    uint32_t ticksPerCycle = Sequencer::ticksPerStep(_tracks[track].rate);
    if (ticksPerCycle == 0) return 0.0f;
    return (float)(_tickCount % ticksPerCycle) / (float)ticksPerCycle;
}

const char* Lfo::waveformLabel(LfoWaveform wf)
{
    switch (wf) {
        case LFO_SINE:     return "SINE";
        case LFO_TRIANGLE: return "TRI";
        case LFO_SQUARE:   return "SQR";
        case LFO_SAW_UP:   return "SAW+";
        case LFO_SAW_DOWN: return "SAW-";
        default:           return "?";
    }
}
