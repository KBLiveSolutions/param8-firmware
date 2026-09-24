#pragma once

#include <Arduino.h>

// Track/output used by the standalone sequencer test boot. Step sequencer
// and LFO share the same track index and output CC/channel, since only one
// of the two is active at a time (toggled with SEQ_BTN_TOGGLE).
#define SEQ_TEST_TRACK 0
#define SEQ_OUTPUT_CC 1
#define SEQ_OUTPUT_CHANNEL 4 // 0-indexed -> displayed as "Ch5"

// Physical encoder assignment for the test boot (indices 4-7 unused).
// Turn function depends on the active mode (step sequencer vs LFO):
//   SEQ_ENC_STEP  -> step select (seq) / waveform (LFO)
//   SEQ_ENC_VALUE -> step value (seq) / center value (LFO)
//   SEQ_ENC_RATE  -> rate, shared meaning in both modes
//   SEQ_ENC_LENGTH -> pattern length (seq) / modulation amount (LFO)
#define SEQ_ENC_STEP 0
#define SEQ_ENC_VALUE 1
#define SEQ_ENC_RATE 2
#define SEQ_ENC_LENGTH 3

// Physical push buttons for the test boot (indices 4,6,7 unused for now).
#define SEQ_BTN_TOGGLE 0 // toggles step sequencer <-> LFO for SEQ_TEST_TRACK
#define SEQ_BTN_CLEAR 3  // resets the current step value (seq) / amount (LFO) to 0

void setupSequencerView();

// Reads the dedicated encoders/buttons for the active mode and applies any
// change. Returns true if something changed (selection, value, length,
// amount, rate or mode).
bool readSequencerEncoders();

// Returns true if the running step/phase or play state changed since the
// last call (used to redraw on clock-driven playback even without encoder
// input).
bool sequencerViewDirty();

void drawSequencerView();
