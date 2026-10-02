#pragma once

#include <Arduino.h>

// Physical encoder assignment (indices 4-7 map to encoders 4 & 5 for rate/length).
// Turn function depends on active mode (step sequencer vs LFO):
//   SEQ_ENC_STEP   -> step select (seq) / waveform (LFO)
//   SEQ_ENC_VALUE  -> step value  (seq) / center value (LFO)
//   SEQ_ENC_RATE   -> rate, shared meaning in both modes
//   SEQ_ENC_LENGTH -> pattern length (seq) / modulation amount (LFO)
#define SEQ_ENC_STEP   0
#define SEQ_ENC_VALUE  1
#define SEQ_ENC_RATE   4
#define SEQ_ENC_LENGTH 5

// Physical push buttons.
#define SEQ_BTN_TOGGLE 0 // toggles LFO <-> SEQ for the active track
#define SEQ_BTN_NEXT   1 // advances selected step (seq mode)
#define SEQ_BTN_CLEAR  3 // fill all steps with current step value (seq) / amount=0 (LFO)
#define SEQ_BTN_STOP   4 // LFO: amount=0 / SEQ: disarm track
#define SEQ_BTN_RND    5 // SEQ: randomise all step values

extern bool sequencerActive;

void enterSequencerFor(uint8_t encoderIdx);
void exitSequencer();

// Reads the dedicated encoders/buttons for the active mode and applies any
// change. Returns true if something changed.
bool readSequencerEncoders();

// Returns true if the running step/phase or play state changed since the
// last call (used to redraw on clock-driven playback even without encoder input).
bool sequencerViewDirty();

// fullRedraw=true: both displays updated. Pass false from clock-driven LFO
// cursor updates to skip display1 (labels don't change).
void drawSequencerView(bool fullRedraw = true);
