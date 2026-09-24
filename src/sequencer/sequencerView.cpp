#include "sequencerView.h"
#include "sequencer.h"
#include "lfo.h"
#include "../input/encoders.h"
#include "../input/buttons.h"
#include "../view/display.h"

static bool lfoMode = false; // false = step sequencer, true = LFO
static uint8_t selectedStep = 0;

static uint8_t lastPlayingStep = 255; // sentinel: forces a first draw
static bool lastRunning = false;
static uint8_t lastLfoPhaseIndex = 255;
static bool lastLfoRunning = false;

// Resolution of the LFO waveform preview: number of sample points/line
// segments spanning one full cycle, drawn on a single screen (display1).
// Also used as the dirty-check granularity for clock-driven redraws.
#define LFO_DISPLAY_RES 64

static void setMode(bool toLfo)
{
    if (toLfo == lfoMode) return;
    lfoMode = toLfo;
    sequencer.arm(SEQ_TEST_TRACK, !toLfo);
    lfo.arm(SEQ_TEST_TRACK, toLfo);
}

void setupSequencerView()
{
    sequencer.setOutput(SEQ_TEST_TRACK, SEQ_OUTPUT_CC, SEQ_OUTPUT_CHANNEL);
    lfo.setOutput(SEQ_TEST_TRACK, SEQ_OUTPUT_CC, SEQ_OUTPUT_CHANNEL);
    sequencer.arm(SEQ_TEST_TRACK, true);
    lfo.arm(SEQ_TEST_TRACK, false);
}

bool readSequencerEncoders()
{
    bool changed = false;

    bool toggleReading = pcf.digitalRead(SEQ_BTN_TOGGLE);
    updateButton(SEQ_BTN_TOGGLE, toggleReading);
    if (wasShortPressed(SEQ_BTN_TOGGLE)) {
        setMode(!lfoMode);
        changed = true;
    }

    bool clearReading = pcf.digitalRead(SEQ_BTN_CLEAR);
    updateButton(SEQ_BTN_CLEAR, clearReading);
    if (wasShortPressed(SEQ_BTN_CLEAR)) {
        if (lfoMode)
            lfo.setAmount(SEQ_TEST_TRACK, 0);
        else
            sequencer.setStepValue(SEQ_TEST_TRACK, selectedStep, 0);
        changed = true;
    }

    if (lfoMode) {
        int wfDelta = encoders.readDelta(SEQ_ENC_STEP);
        if (wfDelta != 0) {
            int wf = ((int)lfo.getWaveform(SEQ_TEST_TRACK) + (wfDelta > 0 ? 1 : -1));
            wf = ((wf % LFO_WAVEFORM_COUNT) + LFO_WAVEFORM_COUNT) % LFO_WAVEFORM_COUNT;
            lfo.setWaveform(SEQ_TEST_TRACK, (LfoWaveform)wf);
            changed = true;
        }

        int valueDelta = encoders.readDeltaVarispeed(SEQ_ENC_VALUE);
        if (valueDelta != 0) {
            int value = (int)lfo.getValue(SEQ_TEST_TRACK) + valueDelta;
            lfo.setValue(SEQ_TEST_TRACK, (uint8_t)constrain(value, 0, 127));
            changed = true;
        }

        int amountDelta = encoders.readDeltaVarispeed(SEQ_ENC_LENGTH);
        if (amountDelta != 0) {
            int amount = (int)lfo.getAmount(SEQ_TEST_TRACK) + amountDelta;
            lfo.setAmount(SEQ_TEST_TRACK, (uint8_t)constrain(amount, 0, 127));
            changed = true;
        }
    } else {
        uint8_t length = sequencer.getLength(SEQ_TEST_TRACK);

        int stepDelta = encoders.readDelta(SEQ_ENC_STEP);
        if (stepDelta != 0) {
            int next = ((int)selectedStep + stepDelta) % length;
            if (next < 0) next += length;
            selectedStep = (uint8_t)next;
            changed = true;
        }

        int valueDelta = encoders.readDeltaVarispeed(SEQ_ENC_VALUE);
        if (valueDelta != 0) {
            int value = (int)sequencer.getStepValue(SEQ_TEST_TRACK, selectedStep) + valueDelta;
            value = constrain(value, 0, 127);
            sequencer.setStepValue(SEQ_TEST_TRACK, selectedStep, (uint8_t)value);
            changed = true;
        }

        int lengthDelta = encoders.readDelta(SEQ_ENC_LENGTH);
        if (lengthDelta != 0) {
            int newLength = (int)length + (lengthDelta > 0 ? 1 : -1);
            newLength = constrain(newLength, 1, SEQ_STEPS);
            sequencer.setLength(SEQ_TEST_TRACK, (uint8_t)newLength);
            if (selectedStep >= newLength)
                selectedStep = newLength - 1;
            changed = true;
        }
    }

    // Rate: shared control, applies to whichever mode is active.
    int rateDelta = encoders.readDelta(SEQ_ENC_RATE);
    if (rateDelta != 0) {
        if (lfoMode) {
            int rate = (int)lfo.getRate(SEQ_TEST_TRACK) + (rateDelta > 0 ? 1 : -1);
            rate = constrain(rate, 0, SEQ_RATE_COUNT - 1);
            lfo.setRate(SEQ_TEST_TRACK, (SeqRate)rate);
        } else {
            int rate = (int)sequencer.getRate(SEQ_TEST_TRACK) + (rateDelta > 0 ? 1 : -1);
            rate = constrain(rate, 0, SEQ_RATE_COUNT - 1);
            sequencer.setRate(SEQ_TEST_TRACK, (SeqRate)rate);
        }
        changed = true;
    }

    return changed;
}

bool sequencerViewDirty()
{
    bool dirty;
    if (lfoMode) {
        float phase = lfo.getPhase(SEQ_TEST_TRACK);
        uint8_t idx = (uint8_t)constrain((int)(phase * LFO_DISPLAY_RES), 0, LFO_DISPLAY_RES - 1);
        bool running = lfo.isRunning();
        dirty = (idx != lastLfoPhaseIndex) || (running != lastLfoRunning);
        lastLfoPhaseIndex = idx;
        lastLfoRunning = running;
    } else {
        uint8_t curStep = sequencer.getCurrentStep(SEQ_TEST_TRACK);
        bool running = sequencer.isRunning();
        dirty = (curStep != lastPlayingStep) || (running != lastRunning);
        lastPlayingStep = curStep;
        lastRunning = running;
    }
    return dirty;
}

// --- Layout: button box (outer edge) / encoder label (inner) / grid+title (center) ---
// mirrored top and bottom, per screen:
//   y=0-9    button box row
//   y=10-17  encoder label row
//   y=17-43  step/LFO grid (ticks + markers)
//   y=46-53  encoder label row
//   y=54-63  button box row

static void drawButtonBox(PicoGFX_SSD1322 &disp, int cellX, int y, const char* label)
{
    const int boxW = 64;
    const int boxH = 9;
    const int boxX = cellX + (128 - boxW) / 2;

    disp.setFont(NULL);
    int tw = getStrWidth(disp, label);
    disp.drawRect(boxX, y, boxW, boxH, 15);
    disp.setCursor(boxX + (boxW - tw) / 2, y + 1);
    disp.setTextColor(15);
    disp.print(label);
}

static void drawEncoderLabel(PicoGFX_SSD1322 &disp, int cellX, int y, const char* label)
{
    disp.setFont(NULL);
    int tw = getStrWidth(disp, label);
    disp.setCursor(cellX + (128 - tw) / 2, y);
    disp.setTextColor(10);
    disp.print(label);
}

static void drawTickAt(PicoGFX_SSD1322 &disp, int col, uint8_t value, int color, bool marker, bool playhead)
{
    const int colW = 32;
    const int margin = 4;
    const int x = col * colW + margin;
    const int tickW = colW - margin * 2;
    const int areaTop = 20;
    const int areaBottom = 40;
    const int areaH = areaBottom - areaTop;

    int y = areaBottom - map(value, 0, 127, 0, areaH);
    disp.fillRect(x, y, tickW, 2, color);

    if (marker)
        disp.fillRect(x, areaBottom + 2, tickW, 1, 15);

    if (playhead)
        disp.fillRect(x, areaTop - 3, tickW, 1, 15);
}

static void drawStepTick(PicoGFX_SSD1322 &disp, int col, uint8_t step)
{
    uint8_t length = sequencer.getLength(SEQ_TEST_TRACK);
    bool active = step < length;
    uint8_t value = active ? sequencer.getStepValue(SEQ_TEST_TRACK, step) : 0;
    bool isSelected = (step == selectedStep);
    bool isPlaying = sequencer.isRunning() && step == sequencer.getCurrentStep(SEQ_TEST_TRACK);
    int color = isSelected ? 15 : (active ? 11 : 4);
    drawTickAt(disp, col, value, color, isSelected, isPlaying);
}

// Draws the LFO waveform as a continuous line across the full width of a
// single display (LFO_DISPLAY_RES sample points spanning one full cycle),
// plus a bright playhead column following the engine's actual phase. Redraws
// live as waveform/value/amount/rate change. Single-screen for now — the
// second display keeps its button/encoder labels but no waveform.
static void drawLfoWaveform(PicoGFX_SSD1322 &disp)
{
    const int width = 256;
    const int areaTop = 17;
    const int areaBottom = 43;
    const int areaH = areaBottom - areaTop;

    int prevX = 0, prevY = areaBottom;
    for (int i = 0; i <= LFO_DISPLAY_RES; i++) {
        float phase = (float)i / (float)LFO_DISPLAY_RES;
        uint8_t value = lfo.previewOutput(SEQ_TEST_TRACK, phase);
        int x = (i * width) / LFO_DISPLAY_RES;
        int y = areaBottom - map(value, 0, 127, 0, areaH);
        if (i > 0)
            disp.drawLine(prevX, prevY, x, y, 11);
        prevX = x;
        prevY = y;
    }

    if (lfo.isRunning()) {
        int x = constrain((int)(lfo.getPhase(SEQ_TEST_TRACK) * width), 0, width - 1);
        disp.fillRect(x, areaTop, 1, areaH, 15);
    }
}

void drawSequencerView()
{
    char labelPos1[16], labelPos2[16], labelPos3[16], labelPos4[16];

    if (lfoMode) {
        snprintf(labelPos1, sizeof(labelPos1), "WAVE %s", Lfo::waveformLabel(lfo.getWaveform(SEQ_TEST_TRACK)));
        snprintf(labelPos2, sizeof(labelPos2), "VAL %d", lfo.getValue(SEQ_TEST_TRACK));
        snprintf(labelPos3, sizeof(labelPos3), "RATE %s", Sequencer::rateLabel(lfo.getRate(SEQ_TEST_TRACK)));
        snprintf(labelPos4, sizeof(labelPos4), "AMT %d", lfo.getAmount(SEQ_TEST_TRACK));
    } else {
        uint8_t length = sequencer.getLength(SEQ_TEST_TRACK);
        snprintf(labelPos1, sizeof(labelPos1), "STEP %d/%d", selectedStep + 1, length);
        snprintf(labelPos2, sizeof(labelPos2), "VAL %d", sequencer.getStepValue(SEQ_TEST_TRACK, selectedStep));
        snprintf(labelPos3, sizeof(labelPos3), "RATE %s", Sequencer::rateLabel(sequencer.getRate(SEQ_TEST_TRACK)));
        snprintf(labelPos4, sizeof(labelPos4), "LEN %d", length);
    }

    const char* runLabel = (lfoMode ? lfo.isRunning() : sequencer.isRunning()) ? "RUN" : "STOP";

    // --- Display 1: positions 1/2 (top), 5/6 (bottom) ---
    display1.fillScreen(0);
    drawButtonBox(display1, 0, 0, "SEQ/LFO");
    drawButtonBox(display1, 128, 0, "-");
    drawEncoderLabel(display1, 0, 10, labelPos1);
    drawEncoderLabel(display1, 128, 10, labelPos2);
    if (lfoMode) {
        drawLfoWaveform(display1);
    } else {
        for (int i = 0; i < 8; i++)
            drawStepTick(display1, i, i);
    }
    drawEncoderLabel(display1, 0, 46, "-");
    drawEncoderLabel(display1, 128, 46, "-");
    drawButtonBox(display1, 0, 54, "MIDI");
    drawButtonBox(display1, 128, 54, "-");

    // --- Display 2: positions 3/4 (top), 7/8 (bottom) ---
    display2.fillScreen(0);
    drawButtonBox(display2, 0, 0, "SYNC");
    drawButtonBox(display2, 128, 0, "CLR");
    drawEncoderLabel(display2, 0, 10, labelPos3);
    drawEncoderLabel(display2, 128, 10, labelPos4);
    if (!lfoMode) {
        for (int i = 0; i < 8; i++)
            drawStepTick(display2, i, i + 8);
    }
    // LFO waveform is shown on display1 only for now (single screen).
    drawEncoderLabel(display2, 0, 46, "-");
    drawEncoderLabel(display2, 128, 46, runLabel);
    drawButtonBox(display2, 0, 54, "-");
    drawButtonBox(display2, 128, 54, "CLR");

    display1.displayBlocking();
    display2.displayBlocking();
}
