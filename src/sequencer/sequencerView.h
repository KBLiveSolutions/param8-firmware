#pragma once

#include <Arduino.h>

// Track/output used by the standalone sequencer test boot.
#define SEQ_TEST_TRACK 0
#define SEQ_OUTPUT_CC 1
#define SEQ_OUTPUT_CHANNEL 4 // 0-indexed -> displayed as "Ch5"

// Physical encoder assignment for the test boot (indices 4-7 unused).
#define SEQ_ENC_STEP 0
#define SEQ_ENC_VALUE 1
#define SEQ_ENC_LENGTH 2
#define SEQ_ENC_RATE 3

// Push button 8 (index 7) exits the test boot.
#define SEQ_EXIT_BUTTON 7

void setupSequencerView();

// Reads the 4 dedicated encoders and applies any change. Returns true if
// something changed (selection, value, length or rate).
bool readSequencerEncoders();

// Reads the EXIT push button. Returns true once on a short press.
bool readSequencerExitButton();

// Returns true if the running step or play state changed since the last
// call (used to redraw on clock-driven playback even without encoder input).
bool sequencerViewDirty();

void drawSequencerView();
