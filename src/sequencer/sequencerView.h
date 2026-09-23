#pragma once

#include <Arduino.h>

// Track/output used by the standalone sequencer test boot.
#define SEQ_TEST_TRACK 0
#define SEQ_OUTPUT_CC 1
#define SEQ_OUTPUT_CHANNEL 4 // 0-indexed -> displayed as "Ch5"

// Physical encoder assignment for the test boot (indices 3-7 unused).
#define SEQ_ENC_STEP 0
#define SEQ_ENC_RATE 1
#define SEQ_ENC_EDIT 2

void setupSequencerView();
void readSequencerEncoders();
void drawSequencerView();
