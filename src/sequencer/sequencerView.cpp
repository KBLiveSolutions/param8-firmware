#include "sequencerView.h"
#include "sequencer.h"
#include "../input/encoders.h"
#include "../input/buttons.h"
#include "../view/display.h"

static uint8_t selectedStep = 0;
static uint8_t lastPlayingStep = 255; // sentinel: forces a first draw
static bool lastRunning = false;

void setupSequencerView()
{
    sequencer.setOutput(SEQ_TEST_TRACK, SEQ_OUTPUT_CC, SEQ_OUTPUT_CHANNEL);
    sequencer.arm(SEQ_TEST_TRACK, true);
}

bool readSequencerEncoders()
{
    bool changed = false;
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

    int rateDelta = encoders.readDelta(SEQ_ENC_RATE);
    if (rateDelta != 0) {
        int rate = (int)sequencer.getRate(SEQ_TEST_TRACK) + (rateDelta > 0 ? 1 : -1);
        rate = constrain(rate, 0, SEQ_RATE_COUNT - 1);
        sequencer.setRate(SEQ_TEST_TRACK, (SeqRate)rate);
        changed = true;
    }

    return changed;
}

bool readSequencerExitButton()
{
    bool reading = pcf.digitalRead(SEQ_EXIT_BUTTON);
    updateButton(SEQ_EXIT_BUTTON, reading);
    return wasShortPressed(SEQ_EXIT_BUTTON);
}

bool sequencerViewDirty()
{
    uint8_t curStep = sequencer.getCurrentStep(SEQ_TEST_TRACK);
    bool running = sequencer.isRunning();
    bool dirty = (curStep != lastPlayingStep) || (running != lastRunning);
    lastPlayingStep = curStep;
    lastRunning = running;
    return dirty;
}

// --- Layout: button box (outer edge) / encoder label (inner) / grid+title (center) ---
// mirrored top and bottom, per screen:
//   y=0-9    button box row
//   y=10-17  encoder label row
//   y=17-43  step grid (ticks + markers)
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

static void drawStepTick(PicoGFX_SSD1322 &disp, int col, uint8_t step)
{
    const int colW = 32;
    const int margin = 4;
    const int x = col * colW + margin;
    const int tickW = colW - margin * 2;
    const int areaTop = 20;
    const int areaBottom = 40;
    const int areaH = areaBottom - areaTop;

    uint8_t length = sequencer.getLength(SEQ_TEST_TRACK);
    bool active = step < length;

    uint8_t value = sequencer.getStepValue(SEQ_TEST_TRACK, step);
    int y = areaBottom - map(value, 0, 127, 0, areaH);

    int color = (step == selectedStep) ? 15 : (active ? 11 : 4);
    disp.fillRect(x, y, tickW, 2, color);

    if (step == selectedStep)
        disp.fillRect(x, areaBottom + 2, tickW, 1, 15);

    if (sequencer.isRunning() && step == sequencer.getCurrentStep(SEQ_TEST_TRACK))
        disp.fillRect(x, areaTop - 3, tickW, 1, 15);
}

void drawSequencerView()
{
    char labelStep[16], labelValue[16], labelLength[16], labelRate[16];
    uint8_t length = sequencer.getLength(SEQ_TEST_TRACK);
    snprintf(labelStep, sizeof(labelStep), "STEP %d/%d", selectedStep + 1, length);
    snprintf(labelValue, sizeof(labelValue), "VAL %d", sequencer.getStepValue(SEQ_TEST_TRACK, selectedStep));
    snprintf(labelLength, sizeof(labelLength), "LEN %d", length);
    snprintf(labelRate, sizeof(labelRate), "RATE %s", Sequencer::rateLabel(sequencer.getRate(SEQ_TEST_TRACK)));

    // --- Display 1: STEP (idx0) / VALUE (idx1) ---
    display1.fillScreen(0);
    drawButtonBox(display1, 0, 0, "-");
    drawButtonBox(display1, 128, 0, "-");
    drawEncoderLabel(display1, 0, 10, labelStep);
    drawEncoderLabel(display1, 128, 10, labelValue);
    for (int i = 0; i < 8; i++)
        drawStepTick(display1, i, i);
    drawEncoderLabel(display1, 0, 46, "-");
    drawEncoderLabel(display1, 128, 46, "-");
    drawButtonBox(display1, 0, 54, "-");
    drawButtonBox(display1, 128, 54, "-");

    // --- Display 2: LENGTH (idx2) / RATE (idx3), EXIT on button 8 (idx7) ---
    display2.fillScreen(0);
    drawButtonBox(display2, 0, 0, "-");
    drawButtonBox(display2, 128, 0, "-");
    drawEncoderLabel(display2, 0, 10, labelLength);
    drawEncoderLabel(display2, 128, 10, labelRate);
    for (int i = 0; i < 8; i++)
        drawStepTick(display2, i, i + 8);
    drawEncoderLabel(display2, 0, 46, "-");
    drawEncoderLabel(display2, 128, 46, sequencer.isRunning() ? "RUN" : "STOP");
    drawButtonBox(display2, 0, 54, "-");
    drawButtonBox(display2, 128, 54, "EXIT");

    display1.displayBlocking();
    display2.displayBlocking();
}
