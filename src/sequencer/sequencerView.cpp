#include "sequencerView.h"
#include "sequencer.h"
#include "../input/encoders.h"
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

    int stepDelta = encoders.readDelta(SEQ_ENC_STEP);
    if (stepDelta != 0) {
        int next = ((int)selectedStep + stepDelta) % SEQ_STEPS;
        if (next < 0) next += SEQ_STEPS;
        selectedStep = (uint8_t)next;
        changed = true;
    }

    int rateDelta = encoders.readDelta(SEQ_ENC_RATE);
    if (rateDelta != 0) {
        int rate = (int)sequencer.getRate(SEQ_TEST_TRACK) + (rateDelta > 0 ? 1 : -1);
        rate = constrain(rate, 0, SEQ_RATE_COUNT - 1);
        sequencer.setRate(SEQ_TEST_TRACK, (SeqRate)rate);
        changed = true;
    }

    int editDelta = encoders.readDelta(SEQ_ENC_EDIT);
    if (editDelta != 0) {
        int value = (int)sequencer.getStepValue(SEQ_TEST_TRACK, selectedStep) + editDelta;
        value = constrain(value, 0, 127);
        sequencer.setStepValue(SEQ_TEST_TRACK, selectedStep, (uint8_t)value);
        changed = true;
    }

    return changed;
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

static void drawStepColumn(PicoGFX_SSD1322 &disp, int col, uint8_t step)
{
    const int colW = 32;
    const int margin = 3;
    const int x = col * colW + margin;
    const int barW = colW - margin * 2;
    const int barAreaTop = 16;
    const int barAreaBottom = 62;
    const int barAreaH = barAreaBottom - barAreaTop;

    uint8_t value = sequencer.getStepValue(SEQ_TEST_TRACK, step);
    int barH = map(value, 0, 127, 0, barAreaH);
    int y = barAreaBottom - barH;

    disp.fillRect(x, y, barW, barH, 10);

    if (step == selectedStep)
        disp.drawRect(x - 2, barAreaTop - 1, barW + 4, barAreaH + 2, 15);

    if (sequencer.isRunning() && step == sequencer.getCurrentStep(SEQ_TEST_TRACK))
        disp.fillRect(x, barAreaTop - 4, barW, 3, 15);
}

static void drawHeader(PicoGFX_SSD1322 &disp, const char* text)
{
    disp.setFont(NULL);
    disp.setCursor(4, 2);
    disp.setTextColor(15);
    disp.print(text);
}

void drawSequencerView()
{
    char buf[32];

    display1.fillScreen(0);
    snprintf(buf, sizeof(buf), "STEP %02d/16   RATE %s", selectedStep + 1, Sequencer::rateLabel(sequencer.getRate(SEQ_TEST_TRACK)));
    drawHeader(display1, buf);
    for (int i = 0; i < 8; i++)
        drawStepColumn(display1, i, i);

    display2.fillScreen(0);
    snprintf(buf, sizeof(buf), "VAL %03d   %s   CC%d/%d",
             sequencer.getStepValue(SEQ_TEST_TRACK, selectedStep),
             sequencer.isRunning() ? "RUN" : "STOP",
             SEQ_OUTPUT_CC, SEQ_OUTPUT_CHANNEL + 1);
    drawHeader(display2, buf);
    for (int i = 0; i < 8; i++)
        drawStepColumn(display2, i, i + 8);

    display1.displayBlocking();
    display2.displayBlocking();
}
