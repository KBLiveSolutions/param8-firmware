#include "sequencerView.h"
#include "sequencer.h"
#include "lfo.h"
#include "../input/encoders.h"
#include "../input/buttons.h"
#include "../view/display.h"
#include "../core/controls.h"
#include "../core/actions.h"
#include "../midi/midi.h"

bool sequencerActive = false;
static bool lfoMode = false;      // false = step sequencer, true = LFO
static uint8_t activeTrack = 0;  // encoder index currently being edited
static uint8_t selectedStep = 0;
static uint8_t entryButtonConsumed = 255; // suppresses the first wasShortPressed for the button that triggered entry

static uint8_t lastPlayingStep = 255; // sentinel: forces a first draw
static bool lastRunning = false;
static bool lastLfoRunning = false;
static unsigned long lastLfoRedrawTime = 0;
#define LFO_REDRAW_INTERVAL_MS 120  // ~8 FPS; display blocking takes ~72ms, leaving loop time for CC


static void setMode(bool toLfo)
{
    if (toLfo == lfoMode) return;
    lfoMode = toLfo;
    sequencer.arm(activeTrack, !toLfo);
    lfo.arm(activeTrack, toLfo);
}

void enterSequencerFor(uint8_t encoderIdx)
{
    activeTrack = encoderIdx;

    if (!lfo.isArmed(activeTrack) && !sequencer.isArmed(activeTrack)) {
        // First entry: initialise output mapping and value from the encoder.
        uint8_t cc  = controls.getEncoder(encoderIdx).number;
        uint8_t ch  = controls.getEncoder(encoderIdx).channel;
        uint8_t val = controls.getEncoder(encoderIdx).value;
        sequencer.setOutput(activeTrack, cc, ch);
        lfo.setOutput(activeTrack, cc, ch);
        lfo.setValue(activeTrack, val);
        lfo.setAmount(activeTrack, 0);
        for (uint8_t s = 0; s < SEQ_STEPS; s++)
            sequencer.setStepValue(activeTrack, s, val);
        lfo.arm(activeTrack, true);
        sequencer.arm(activeTrack, false);
        lfoMode = true;
    } else {
        // Track already running: keep its CC/Ch, just restore the display mode.
        lfoMode = lfo.isArmed(activeTrack);
    }

    selectedStep = 0;
    sequencerActive = true;
    entryButtonConsumed = encoderIdx;
    drawSequencerView();
}

void exitSequencer()
{
    sequencerActive = false;
    updateFaderTitles();
    updateFaderValues();
    showDisplay();
}

bool readSequencerEncoders()
{
    bool changed = false;

    // readButtons() runs before readSequencerEncoders() in the main loop.
    // The shift+push that entered the sequencer fires a button release on the
    // same frame, setting shortPressEventPending for that button. Consume it
    // silently so it doesn't trigger a sequencer action (e.g. CLEAR/STOP).
    auto guardedPressed = [](uint8_t btn) -> bool {
        if (!wasShortPressed(btn)) return false;
        if (btn == entryButtonConsumed) { entryButtonConsumed = 255; return false; }
        return true;
    };

    bool toggleReading = pcf.digitalRead(SEQ_BTN_TOGGLE);
    updateButton(SEQ_BTN_TOGGLE, toggleReading);
    if (guardedPressed(SEQ_BTN_TOGGLE)) {
        setMode(!lfoMode);
        changed = true;
    }

    bool nextReading = pcf.digitalRead(SEQ_BTN_NEXT);
    updateButton(SEQ_BTN_NEXT, nextReading);
    if (guardedPressed(SEQ_BTN_NEXT) && !lfoMode) {
        uint8_t len = sequencer.getLength(activeTrack);
        selectedStep = (selectedStep + 1) % len;
        changed = true;
    }

    bool clearReading = pcf.digitalRead(SEQ_BTN_CLEAR);
    updateButton(SEQ_BTN_CLEAR, clearReading);
    if (guardedPressed(SEQ_BTN_CLEAR)) {
        if (lfoMode) {
            lfo.setAmount(activeTrack, 0);
        } else {
            uint8_t val = sequencer.getStepValue(activeTrack, selectedStep);
            for (uint8_t s = 0; s < SEQ_STEPS; s++)
                sequencer.setStepValue(activeTrack, s, val);
        }
        changed = true;
    }

    bool stopReading = pcf.digitalRead(SEQ_BTN_STOP);
    updateButton(SEQ_BTN_STOP, stopReading);
    if (guardedPressed(SEQ_BTN_STOP)) {
        if (lfoMode) {
            lfo.setAmount(activeTrack, 0);
        } else {
            sequencer.arm(activeTrack, false);
            lfo.arm(activeTrack, false);
        }
        changed = true;
    }

    bool rndReading = pcf.digitalRead(SEQ_BTN_RND);
    updateButton(SEQ_BTN_RND, rndReading);
    if (guardedPressed(SEQ_BTN_RND) && !lfoMode) {
        for (uint8_t s = 0; s < SEQ_STEPS; s++)
            sequencer.setStepValue(activeTrack, s, (uint8_t)random(128));
        changed = true;
    }

    if (lfoMode) {
        int wfDelta = encoders.readDelta(SEQ_ENC_STEP);
        if (wfDelta != 0) {
            int wf = constrain((int)lfo.getWaveform(activeTrack) + (wfDelta > 0 ? 1 : -1),
                               0, LFO_WAVEFORM_COUNT - 1);
            lfo.setWaveform(activeTrack, (LfoWaveform)wf);
            changed = true;
        }

        int valueDelta = encoders.readDeltaVarispeed(SEQ_ENC_VALUE);
        if (valueDelta != 0) {
            int value = (int)lfo.getValue(activeTrack) + valueDelta;
            lfo.setValue(activeTrack, (uint8_t)constrain(value, 0, 127));
            changed = true;
        }

        int amountDelta = encoders.readDeltaVarispeed(SEQ_ENC_LENGTH);
        if (amountDelta != 0) {
            int amount = (int)lfo.getAmount(activeTrack) + amountDelta;
            lfo.setAmount(activeTrack, (int8_t)constrain(amount, -127, 127));
            changed = true;
        }
    } else {
        uint8_t length = sequencer.getLength(activeTrack);

        int stepDelta = encoders.readDelta(SEQ_ENC_STEP);
        if (stepDelta != 0) {
            int next = ((int)selectedStep + stepDelta) % length;
            if (next < 0) next += length;
            selectedStep = (uint8_t)next;
            changed = true;
        }

        int valueDelta = encoders.readDeltaVarispeed(SEQ_ENC_VALUE);
        if (valueDelta != 0) {
            int value = (int)sequencer.getStepValue(activeTrack, selectedStep) + valueDelta;
            value = constrain(value, 0, 127);
            sequencer.setStepValue(activeTrack, selectedStep, (uint8_t)value);
            changed = true;
        }

        int lengthDelta = encoders.readDelta(SEQ_ENC_LENGTH);
        if (lengthDelta != 0) {
            int newLength = (int)length + (lengthDelta > 0 ? 1 : -1);
            newLength = constrain(newLength, 1, SEQ_STEPS);
            sequencer.setLength(activeTrack, (uint8_t)newLength);
            if (selectedStep >= newLength)
                selectedStep = newLength - 1;
            changed = true;
        }
    }

    // Rate (LFO: freq varispeed) / Rate (seq: inverted, up=slower).
    if (lfoMode) {
        int rateDelta = encoders.readDeltaVarispeed(SEQ_ENC_RATE);
        if (rateDelta != 0) {
            float freq = lfo.getFreq(activeTrack);
            freq = constrain(freq * powf(1.15f, (float)rateDelta), 0.05f, 2.0f);
            lfo.setFreq(activeTrack, freq);
            changed = true;
        }
    } else {
        int rateDelta = encoders.readDelta(SEQ_ENC_RATE);
        if (rateDelta != 0) {
            // encoder up = slower (toward 4 BARS = lower index)
            int rate = (int)sequencer.getRate(activeTrack) - (rateDelta > 0 ? 1 : -1);
            rate = constrain(rate, 0, SEQ_RATE_COUNT - 1);
            sequencer.setRate(activeTrack, (SeqRate)rate);
            changed = true;
        }
    }

    return changed;
}

bool sequencerViewDirty()
{
    bool dirty;
    if (lfoMode) {
        unsigned long now = millis();
        bool running = lfo.isRunning();
        dirty = (now - lastLfoRedrawTime >= LFO_REDRAW_INTERVAL_MS) || (running != lastLfoRunning);
        if (dirty) {
            lastLfoRedrawTime = now;
            lastLfoRunning = running;
        }
    } else {
        uint8_t curStep = sequencer.getCurrentStep(activeTrack);
        bool running = sequencer.isRunning();
        dirty = (curStep != lastPlayingStep) || (running != lastRunning);
        lastPlayingStep = curStep;
        lastRunning = running;
    }
    return dirty;
}

// Display1 layout: button boxes at y=0 and y=54, compact fader widgets fill y=10-53.
// Display2 layout: full-height visualization (y=5-58).

static void drawToggleBox(PicoGFX_SSD1322 &disp, int cellX, int y, bool leftActive)
{
    const int totalW = 64;
    const int boxH   = 9;
    const int halfW  = totalW / 2;
    const int boxX   = cellX + (128 - totalW) / 2;

    disp.setFont(NULL);

    // Fill the full box rounded, then erase the inactive half so only the
    // active side keeps white fill (rounded outer corners, flat inner edge).
    disp.fillRoundRect(boxX, y, totalW, boxH, BUTTON_CORNER, 15);
    if (leftActive)
        disp.fillRect(boxX + halfW, y, halfW, boxH, 0);
    else
        disp.fillRect(boxX,         y, halfW, boxH, 0);

    disp.drawRoundRect(boxX, y, totalW, boxH, BUTTON_CORNER, BUTTON_BORDER_COLOR);
    disp.drawLine(boxX + halfW, y, boxX + halfW, y + boxH - 1, BUTTON_BORDER_COLOR);

    disp.setTextColor(leftActive ? 0 : 8);
    int tw = getStrWidth(disp, "SEQ");
    disp.setCursor(boxX + (halfW - tw) / 2, y + 1);
    disp.print("SEQ");

    disp.setTextColor(!leftActive ? 0 : 8);
    tw = getStrWidth(disp, "LFO");
    disp.setCursor(boxX + halfW + (halfW - tw) / 2, y + 1);
    disp.print("LFO");
}

static void drawButtonBox(PicoGFX_SSD1322 &disp, int cellX, int y, const char* label)
{
    const int boxW = 64;
    const int boxH = 9;
    const int boxX = cellX + (128 - boxW) / 2;
    disp.setFont(NULL);
    int tw = getStrWidth(disp, label);
    disp.drawRoundRect(boxX, y, boxW, boxH, BUTTON_CORNER, BUTTON_BORDER_COLOR);
    disp.setCursor(boxX + (boxW - tw) / 2, y + 1);
    disp.setTextColor(15);
    disp.print(label);
}


static void drawSeqGrid(PicoGFX_SSD1322 &disp)
{
    const int axisX      = 21;
    const int startX     = axisX + 1;
    const int waveW      = 256 - startX;
    const int areaTop    = 5;
    const int areaBottom = 58;
    const int areaH      = areaBottom - areaTop;

    disp.setFont(NULL);
    disp.setTextColor(5);

    struct { uint8_t val; const char* lbl; } refs[] = {{127,"127"},{63,"63"},{0,"0"}};
    for (auto &r : refs) {
        int y = areaBottom - map(r.val, 0, 127, 0, areaH);
        disp.drawLine(startX, y, 255, y, 4);
        int tw = getStrWidth(disp, r.lbl);
        disp.setCursor(axisX - tw - 1, y - 3);
        disp.print(r.lbl);
    }

    disp.drawLine(axisX, areaTop, axisX, areaBottom, 5);

    uint8_t length  = sequencer.getLength(activeTrack);
    uint8_t curStep = sequencer.getCurrentStep(activeTrack);
    bool    running = sequencer.isRunning();

    for (int i = 0; i < SEQ_STEPS; i++) {
        int x    = startX + (i * waveW) / SEQ_STEPS + 1;
        int xEnd = startX + ((i + 1) * waveW) / SEQ_STEPS - 1;
        int tw   = max(xEnd - x, 1);

        bool active     = i < (int)length;
        uint8_t value   = active ? sequencer.getStepValue(activeTrack, i) : 0;
        bool isSelected = (i == (int)selectedStep);
        bool isPlaying  = running && (i == (int)curStep);
        int color       = isSelected ? 15 : (active ? 11 : 4);

        int y = areaBottom - map(value, 0, 127, 0, areaH);
        disp.fillRect(x, y, tw, 2, color);

        if (isSelected)
            disp.fillRect(x, areaBottom + 2, tw, 1, 15);
        if (isPlaying)
            disp.fillRect(x, areaTop, tw, 2, 15);
    }
}

// Draws the LFO waveform as a continuous line across the full width of a
// single display (LFO_DISPLAY_RES sample points spanning one full cycle),
// plus a bright playhead column following the engine's actual phase. Redraws
// live as waveform/value/amount/rate change. Single-screen for now — the
// second display keeps its button/encoder labels but no waveform.
static void drawLfoWaveform(PicoGFX_SSD1322 &disp)
{
    static const int N = 64;

    const int axisX      = 21;
    const int waveX      = axisX + 1;
    const int waveW      = 256 - waveX;
    const int areaTop    = 5;
    const int areaBottom = 58;
    const int areaH      = areaBottom - areaTop;

    disp.setFont(NULL);
    disp.setTextColor(5);

    struct { uint8_t val; const char* lbl; } refs[] = {{127,"127"},{63,"63"},{0,"0"}};
    for (auto &r : refs) {
        int y = areaBottom - map(r.val, 0, 127, 0, areaH);
        disp.drawLine(waveX, y, 255, y, 4);
        int tw = getStrWidth(disp, r.lbl);
        disp.setCursor(axisX - tw - 1, y - 3);
        disp.print(r.lbl);
    }
    disp.drawLine(axisX, areaTop, axisX, areaBottom, 5);

    // Phase window centred on currentPhase: bar i=N/2 == currentPhase,
    // bars 0..(N/2-1) are the past half-cycle, bars (N/2+1)..(N-1) the future.
    float currentPhase = lfo.getPhase(activeTrack);
    for (int i = 0; i < N; i++) {
        float phase = fmodf(currentPhase + (float)i / (float)N - 0.5f + 2.0f, 1.0f);
        uint8_t value = lfo.previewOutput(activeTrack, phase);
        int x   = waveX + (i       * waveW) / N;
        int xNx = waveX + ((i + 1) * waveW) / N;
        int bw  = max(xNx - x, 1);
        int y   = areaBottom - map(value, 0, 127, 0, areaH);
        int bh  = areaBottom - y;
        disp.fillRect(x, y,     bw, 1,       11);
        if (bh > 1) disp.fillRect(x, y + 1, bw, bh - 1, 4);
    }

    // Fixed dot at centre X, tracking the current output value.
    int dotX = waveX + waveW / 2;
    int dotY = areaBottom - map(lfo.previewOutput(activeTrack, currentPhase), 0, 127, 0, areaH);
    disp.fillRect(dotX - 1, dotY - 1, 3, 3, 15);
}

void drawSequencerView(bool fullRedraw)
{
    // --- Display 2 (right): full-height visualization ---
    display2.fillScreen(0);
    if (lfoMode)
        drawLfoWaveform(display2);
    else
        drawSeqGrid(display2);

    // Display2 always refreshes (cursor/playhead moves on every dirty frame).
    display2.display();
    while (!display2.isTransferComplete()) {
        yield();
        midiRead();
        lfo.onTimer();
    }

    // Display1 (left, compact encoder layout) only needs updating when a
    // parameter actually changed — skip it on cursor-only frames.
    if (!fullRedraw) return;

    // --- Display 1 (left): button boxes + compact fader widgets ---
    display1.fillScreen(0);
    drawToggleBox(display1, 0, 0, !lfoMode);
    drawButtonBox(display1, 128, 0,  lfoMode ? "-" : "NEXT");
    drawButtonBox(display1, 0,   54, "STOP");
    drawButtonBox(display1, 128, 54, lfoMode ? "-" : "RND");

    char title0[20], title1[20], title4[20], title5[20];

    if (lfoMode) {
        uint8_t wf  = (uint8_t)lfo.getWaveform(activeTrack);
        uint8_t val = lfo.getValue(activeTrack);
        float   freq = lfo.getFreq(activeTrack);
        int8_t  amt = lfo.getAmount(activeTrack);

        snprintf(title0, sizeof(title0), "%s", Lfo::waveformLabel((LfoWaveform)wf));
        snprintf(title1, sizeof(title1), "%d", val);
        if (freq < 1.0f) snprintf(title4, sizeof(title4), "%.2fHz", freq);
        else             snprintf(title4, sizeof(title4), "%.1fHz", freq);
        snprintf(title5, sizeof(title5), "%d", amt);

        strncpy(faders[0]->paramName, "Wave",   sizeof(faders[0]->paramName));
        strncpy(faders[1]->paramName, "Value",  sizeof(faders[1]->paramName));
        strncpy(faders[4]->paramName, "Freq",   sizeof(faders[4]->paramName));
        strncpy(faders[5]->paramName, "Amount", sizeof(faders[5]->paramName));

        faders[0]->value = (wf * 127) / (LFO_WAVEFORM_COUNT - 1);
        faders[1]->value = val;
        faders[4]->value = (int)constrain(
            (logf(freq) - logf(0.05f)) / (logf(2.0f) - logf(0.05f)) * 127.0f, 0, 127);
        faders[5]->value = (amt + 127) / 2;
    } else {
        uint8_t length = sequencer.getLength(activeTrack);
        uint8_t val    = sequencer.getStepValue(activeTrack, selectedStep);
        uint8_t rate   = (uint8_t)sequencer.getRate(activeTrack);

        snprintf(title0, sizeof(title0), "%d/%d", selectedStep + 1, length);
        snprintf(title1, sizeof(title1), "%d", val);
        snprintf(title4, sizeof(title4), "%s", Sequencer::rateLabel((SeqRate)rate));
        snprintf(title5, sizeof(title5), "%d", length);

        strncpy(faders[0]->paramName, "Step",   sizeof(faders[0]->paramName));
        strncpy(faders[1]->paramName, "Value",  sizeof(faders[1]->paramName));
        strncpy(faders[4]->paramName, "Rate",   sizeof(faders[4]->paramName));
        strncpy(faders[5]->paramName, "Length", sizeof(faders[5]->paramName));

        faders[0]->value = length > 1 ? (selectedStep * 127) / (length - 1) : 0;
        faders[1]->value = val;
        faders[4]->value = ((SEQ_RATE_COUNT - 1 - rate) * 127) / (SEQ_RATE_COUNT - 1);
        faders[5]->value = ((length - 1) * 127) / (SEQ_STEPS - 1);
    }

    strncpy(faders[0]->title, title0, sizeof(faders[0]->title));
    strncpy(faders[1]->title, title1, sizeof(faders[1]->title));
    strncpy(faders[4]->title, title4, sizeof(faders[4]->title));
    strncpy(faders[5]->title, title5, sizeof(faders[5]->title));

    faders[0]->drawFaderCompact();
    faders[1]->drawFaderCompact();
    faders[4]->drawFaderCompact();
    faders[5]->drawFaderCompact();

    display1.display();
    while (!display1.isTransferComplete()) {
        yield();
        midiRead();
        lfo.onTimer();
    }
}
