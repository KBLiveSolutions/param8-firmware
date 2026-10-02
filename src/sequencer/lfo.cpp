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
        _phase[i] = 0.0f;
        _shValue[i] = 0.0f;
    }
    _lastMs = 0;
}

float Lfo::waveformValue(LfoWaveform wf, float phase)
{
    switch (wf) {
        case LFO_SINE:
            return sinf(2.0f * (float)M_PI * phase);
        case LFO_TRIANGLE:
            return 1.0f - 4.0f * fabsf(fmodf(phase + 0.25f, 1.0f) - 0.5f);
        case LFO_SQUARE:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LFO_SAW:
            return 2.0f * phase - 1.0f;
        default:
            return 0.0f;
    }
}

uint8_t Lfo::previewOutput(uint8_t track, float phase) const
{
    if (track >= SEQ_TRACKS) return 0;
    const LfoTrack &t = _tracks[track];
    float wave = (t.waveform == LFO_SAMPLE_HOLD) ? _shValue[track]
                                                  : waveformValue(t.waveform, phase);
    int output = (int)t.value + (int)roundf(wave * (float)t.amount);
    return (uint8_t)constrain(output, 0, 127);
}

void Lfo::onTimer()
{
    uint32_t now = (uint32_t)millis();
    if (_lastMs == 0) { _lastMs = now; return; }
    float dt = (float)(now - _lastMs) / 1000.0f;
    if (dt == 0.0f) return;  // sub-ms call, nothing to do
    _lastMs = now;

    // Pass 1: advance all phases (no MIDI send here).
    for (uint8_t i = 0; i < SEQ_TRACKS; i++) {
        if (!_tracks[i].armed) continue;
        _phase[i] += _tracks[i].freq * dt;
        while (_phase[i] >= 1.0f) {
            _phase[i] -= 1.0f;
            if (_tracks[i].waveform == LFO_SAMPLE_HOLD)
                _shValue[i] = ((float)random(201) / 100.0f) - 1.0f;
        }
    }

    // Pass 2: send at most ONE CC per call (round-robin across tracks).
    // Prevents back-to-back writePacket() calls from saturating the TinyUSB
    // FIFO and blocking the main loop.
    for (uint8_t j = 0; j < SEQ_TRACKS; j++) {
        uint8_t i = (_sendCursor + j) % SEQ_TRACKS;
        if (!_tracks[i].armed) continue;
        uint8_t output = previewOutput(i, _phase[i]);
        if (output != _lastOutput[i]) {
            _lastOutput[i] = output;
            if ((now - _lastSendMs[i]) >= 5) {
                _lastSendMs[i] = now;
                sendMidiMessage(MIDI_CC, _tracks[i].ccNumber, output, _tracks[i].channel);
                _sendCursor = (i + 1) % SEQ_TRACKS;
                return;
            }
        }
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

void Lfo::setFreq(uint8_t track, float hz)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].freq = hz;
}

float Lfo::getFreq(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].freq : 1.0f;
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

void Lfo::setAmount(uint8_t track, int8_t amount)
{
    if (track >= SEQ_TRACKS) return;
    int v = (int)_tracks[track].value;
    int maxAmt = min(v, 127 - v);
    int a = constrain((int)amount, -maxAmt, maxAmt);
    _tracks[track].amount = (int8_t)a;
}

int8_t Lfo::getAmount(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].amount : 0;
}

void Lfo::setOutput(uint8_t track, uint8_t ccNumber, uint8_t channel)
{
    if (track >= SEQ_TRACKS) return;
    _tracks[track].ccNumber = ccNumber;
    _tracks[track].channel = channel;
}

uint8_t Lfo::getCCNumber(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].ccNumber : 0;
}

uint8_t Lfo::getCCChannel(uint8_t track) const
{
    return track < SEQ_TRACKS ? _tracks[track].channel : 0;
}

uint8_t Lfo::getCurrentOutput(uint8_t track) const
{
    return track < SEQ_TRACKS ? _lastOutput[track] : 0;
}

float Lfo::getPhase(uint8_t track) const
{
    return track < SEQ_TRACKS ? _phase[track] : 0.0f;
}

const char* Lfo::waveformLabel(LfoWaveform wf)
{
    switch (wf) {
        case LFO_SINE:     return "SINE";
        case LFO_TRIANGLE: return "TRI";
        case LFO_SQUARE:   return "SQR";
        case LFO_SAW:         return "SAW";
        case LFO_SAMPLE_HOLD: return "S&H";
        default:              return "?";
    }
}
