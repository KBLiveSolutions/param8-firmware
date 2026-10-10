#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"

#define MAX_LATCH_EVENTS 256
#define SHIFT_HOLD_MS     300
#define SHIFT_DTAP_MS     400
#define SHIFT_DTHOLD_MS   500

uint8_t presetTable = 0;
bool shiftPressed = false;
enum ShiftState : uint8_t { SHIFT_IDLE, SHIFT_DOWN1, SHIFT_HOLD, SHIFT_UP1, SHIFT_DOWN2 };
static ShiftState shiftState = SHIFT_IDLE;
static unsigned long shiftStateTime = 0;
bool latchPressed = false;
uint8_t latchEncoderEvents[8][MAX_LATCH_EVENTS] = {{0}};
uint8_t latchEncoderEventCount[8] = {0};
bool revertMode = false;
unsigned long lastInputTime = 0;
unsigned long lastButtonReleaseTime[8] = {0};
uint8_t lastControlIdx = 0;
bool lastControlIsButton = false;
unsigned long lastLatchPressTime = 0;
bool latchPendingActivation = false;
bool latchHeld = false;
bool namingPendingConfirm = false;
unsigned long namingConfirmTime = 0;

uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

static uint8_t latchOriginalValue[8] = {0};

static const int slewStepsMs[] = {0, 100, 200, 500, 1000, 2000};
#define SLEW_STEPS 6
uint8_t latchSlewLevel  = 0;
uint8_t revertSlewLevel = 0;

uint8_t revertAbsoluteOriginal[8] = {0};
bool revertAbsoluteChanged[8] = {false};

static int latchRelAccum[8] = {0};

static void sendAbsoluteSlew(uint8_t idx, int fromVal, int toVal, int slewMs) {
    uint8_t  channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t  number  = controls.getEncoder(idx).number;
    int range = abs(toVal - fromVal);
    if (range == 0) { sendMidiMessage(type, number, (uint8_t)toVal, channel); return; }
    int totalMs = (slewMs * range + 63) / 127;
    const int stepMs = 5;
    int steps = max(1, totalMs / stepMs);
    for (int s = 1; s <= steps; s++) {
        int v = fromVal + ((toVal - fromVal) * s) / steps;
        sendMidiMessage(type, number, (uint8_t)constrain(v, 0, 127), channel);
        delay(stepMs);
    }
}

void initActionSettings() {
    int savedLS = json.getDoc()["latchSlew"]  | 0;
    int savedRS = json.getDoc()["revertSlew"] | 0;
    latchSlewLevel  = (uint8_t)constrain(savedLS, 0, SLEW_STEPS - 1);
    revertSlewLevel = (uint8_t)constrain(savedRS, 0, SLEW_STEPS - 1);
}

static void drawPresetPage()
{
    display1.fillScreen(0);
    display2.fillScreen(0);
    for (int i = 0; i < 8; ++i) {
        char l1[20], l2[20] = {0};
        if (i == 6)      { strcpy(l1, "Global"); strcpy(l2, "Mode"); }
        else if (i == 7) { strcpy(l1, "Device"); strcpy(l2, "Mode"); }
        else {
            uint8_t presetIdx = presetTable * 6 + i;
            snprintf(l1, sizeof(l1), "Preset %d", presetIdx + 1);
            const char* pname = controls.getPresetName(presetIdx);
            if (pname[0] != '\0') strncpy(l2, pname, sizeof(l2) - 1);
        }
        bool isActive;
        if (i == 6) isActive = (controls.getPreset() == 24);
        else if (i == 7) isActive = (controls.getPreset() == 25);
        else isActive = ((presetTable * 6 + i) == controls.getPreset());
        faders[i]->drawPresetButton(l1, l2, isActive);
    }
    flushDisplays();
}

static void startFirstPress(unsigned long now) {
    shiftState = SHIFT_DOWN1;
    shiftStateTime = now;
    shiftPressed = true;
    setLed(1, true);
    sendMidiMessage(0, 110, 127, 7);
    if (revertMode) {
        if (controls.getPreset() > 23) {
            uint8_t pkt[5] = {240, 111, 0x1A, 2, 247};
            usb_midi.write(pkt, 5);
        }
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i)
            revertAbsoluteChanged[i] = false;
    }
    if (controls.getPreset() < 24) presetTable = controls.getPreset() / 6;
}

void onShiftPress()
{
    unsigned long now = millis();
    lastInputTime = now;

    if (latchPressed) {
        bool isUserPreset = (controls.getPreset() < 24);
        latchPressed = false;
        latchPendingActivation = false;
        latchHeld = false;
        setLed(0, false);
        for (int i = 0; i < 8; ++i) {
            latchEncoderEventCount[i] = 0;
            latchRelAccum[i] = 0;
            latchAbsoluteChanged[i] = false;
        }
        if (isUserPreset) {
            for (int i = 0; i < 8; ++i)
                controls.getEncoder(i).value = latchOriginalValue[i];
            updateFaderValues();
            showDisplay();
            for (int i = 0; i < 8; ++i)
                faders[i]->updateButtonName(faders[i]->buttonState);
            flushDisplays();
        } else {
            sendPresetSysEx(controls.getPreset());
        }
        return;
    }

    switch (shiftState) {
    case SHIFT_IDLE:
        startFirstPress(now);
        break;
    case SHIFT_UP1:
        if (now - shiftStateTime < SHIFT_DTAP_MS) {
            shiftState = SHIFT_DOWN2;
            shiftStateTime = now;
            shiftPressed = true;
        } else {
            startFirstPress(now);
        }
        break;
    case SHIFT_DOWN1:
        if (now - shiftStateTime < SHIFT_HOLD_MS) {
            shiftPressed = false;
            setLed(1, false);
            sendMidiMessage(0, 110, 0, 7);
            shiftState = SHIFT_DOWN2;
            shiftStateTime = now;
        }
        break;
    case SHIFT_HOLD:
    case SHIFT_DOWN2:
        break;
    }
}

void onShiftRelease()
{
    unsigned long now = millis();

    switch (shiftState) {
    case SHIFT_DOWN1:
        shiftPressed = false;
        setLed(1, false);
        sendMidiMessage(0, 110, 0, 7);
        shiftState = SHIFT_IDLE;
        showDisplay();
        for (int i = 0; i < 8; ++i)
            faders[i]->updateButtonName(faders[i]->buttonState);
        flushDisplays();
        break;

    case SHIFT_HOLD:
        shiftState = SHIFT_IDLE;
        shiftPressed = false;
        setLed(1, false);
        sendMidiMessage(0, 110, 0, 7);
        showDisplay();
        for (int i = 0; i < 8; ++i)
            faders[i]->updateButtonName(faders[i]->buttonState);
        flushDisplays();
        break;

    case SHIFT_DOWN2:
        shiftPressed = false;
        shiftState = SHIFT_IDLE;
        break;

    default:
        shiftPressed = false;
        shiftState = SHIFT_IDLE;
        break;
    }
}

void sendNameRequest(uint8_t idx, uint8_t isButton)
{
    MidiControl& ctrl = isButton
        ? controls.getButtonShort(idx)
        : controls.getEncoder(idx);
    uint8_t packet[9] = {240, 111, 17, idx, isButton, (uint8_t)ctrl.type, ctrl.number, ctrl.channel, 247};
    usb_midi.write(packet, 9);
}

void sendClearNaming(uint8_t idx, uint8_t isButton)
{
    uint8_t preset = controls.getPreset();
    if (preset >= 24) return;

    MidiControl& ctrl = isButton
        ? controls.getButtonShort(idx)
        : controls.getEncoder(idx);
    ctrl.controlName[0] = '\0';
    if (!isButton)
        ctrl.hasWatcher = false;

    updateFaderTitles();
    showDisplay();

    uint8_t packet[7] = {240, 111, 0x19, preset, idx, isButton, 247};
    usb_midi.write(packet, 7);
}

void checkNamingPending()
{
    if (!namingPendingConfirm)
        return;
    if (millis() - namingConfirmTime >= 1000)
    {
        namingPendingConfirm = false;
        sendClearNaming(lastControlIdx, lastControlIsButton ? 1 : 0);
    }
}

void checkShiftPending()
{
    unsigned long now = millis();
    if (shiftState == SHIFT_DOWN1 && now - shiftStateTime >= SHIFT_HOLD_MS) {
        shiftState = SHIFT_HOLD;
        drawPresetPage();
    }
}

void checkLatchPending()
{
    if (!latchPendingActivation)
        return;
    if (millis() - lastLatchPressTime < 300)
        return;
    latchPendingActivation = false;
    if (latchHeld)
    {
        latchPressed = true;
        setLed(0, true);
    }
}

void onLatchPress()
{
    unsigned long now = millis();
    latchHeld = true;
    if (latchPendingActivation && now - lastLatchPressTime < 300)
    {
        latchPendingActivation = false;
        namingPendingConfirm = true;
        namingConfirmTime = now;
        lastLatchPressTime = 0;
        return;
    }
    lastLatchPressTime = now;
    latchPendingActivation = true;
    for (int i = 0; i < 8; ++i) {
        latchEncoderEventCount[i] = 0;
        latchRelAccum[i] = 0;
        latchAbsoluteChanged[i] = false;
        latchOriginalValue[i] = controls.getEncoder(i).value;
    }

    if (revertMode && !shiftPressed)
    {
        latchPendingActivation = false;
        if (controls.getPreset() > 23)
        {
            uint8_t pkt[5] = {240, 111, 0x1A, 0, 247};
            usb_midi.write(pkt, 5);
        }
        else
            sendRevertEvents();
        revertMode = false;
        setRevertModeLed(false);
        return;
    }

    if (shiftPressed)
    {
        if (!revertMode)
        {
            revertMode = true;
            for (int i = 0; i < 8; ++i)
            {
                revertAbsoluteOriginal[i] = controls.getEncoder(i).value;
                revertAbsoluteChanged[i] = false;
            }
            if (controls.getPreset() > 23)
            {
                uint8_t pkt[5] = {240, 111, 0x1A, 1, 247};
                usb_midi.write(pkt, 5);
            }
            setRevertModeLed(true);
        }
    }
}

void onLatchRelease()
{
    latchHeld = false;
    if (namingPendingConfirm)
    {
        namingPendingConfirm = false;
        sendNameRequest(lastControlIdx, lastControlIsButton ? 1 : 0);
        return;
    }
    if (latchPendingActivation)
        return;
    if (!latchPressed)
        return;
    latchPressed = false;
    setLed(0, false);
    releaseLatchAndSend();
}

void onButtonShortPress(uint8_t idx)
{
}

void onButtonPressed(uint8_t idx)
{
    lastControlIdx = idx;
    lastControlIsButton = true;
    lastInputTime = millis();
    if (screenSaverActive)
    {
        screenSaverActive = false;
        showDisplay();
    }
    if (shiftState == SHIFT_HOLD)
    {
        uint8_t newPreset;
        char l1[20], l2[20] = {0};
        if (idx == 6) {
            newPreset = 24;
            strcpy(l1, "Global"); strcpy(l2, "Mode");
        } else if (idx == 7) {
            newPreset = 25;
            strcpy(l1, "Device"); strcpy(l2, "Mode");
        } else {
            newPreset = presetTable * 6 + idx;
            snprintf(l1, sizeof(l1), "Preset %d", newPreset + 1);
            const char* pname = controls.getPresetName(newPreset);
            if (pname[0] != '\0') strncpy(l2, pname, sizeof(l2) - 1);
        }
        faders[idx]->drawPresetButton(l1, l2, true);
        flushDisplays();
        controls.setPreset(newPreset);
        updateFaderTitles();
        updateFaderValues();
        showDisplay();
        sendPresetSysEx(newPreset);
        return;
    }
    else
    {
        uint8_t channel = controls.getButtonShort(idx).channel;
        ControlMidiType type = controls.getButtonShort(idx).type;
        uint8_t number = controls.getButtonShort(idx).number;
        uint8_t _value;
        bool isToggle = (controls.getPreset() < 24) && controls.getButtonShort(idx).toggleMode;
        if (isToggle) {
            bool newState = !faders[idx]->buttonState;
            faders[idx]->buttonState = newState;
            faders[idx]->updateButtonName(newState);
            if (controls.getPreset() < 24) {
                _value = newState ? controls.getButtonShort(idx).maxVal : controls.getButtonShort(idx).minVal;
            } else {
                _value = newState ? 127 : 0;
            }
        } else {
            _value = (controls.getPreset() < 24) ? controls.getButtonShort(idx).maxVal : 127;
        }
        controls.getButtonShort(idx).value = _value;
        sendMidiMessage(type, number, _value, channel);
        if (controls.getPreset() == 24) {
            if (idx == 3 || idx == 7) {
                faders[idx]->updateButtonName(true);
            } else {
                bool on = !faders[idx]->buttonState;
                faders[idx]->buttonState = on;
                faders[idx]->updateButtonName(on);
            }
        }
        if (controls.getPreset() == 25) {
            if (idx != 2 && idx != 3) {
                faders[idx]->updateButtonName(true);
            }
        }
    }
}

void onButtonReleased(uint8_t idx)
{
    uint8_t channel = controls.getButtonShort(idx).channel;
    ControlMidiType type = controls.getButtonShort(idx).type;
    uint8_t number = controls.getButtonShort(idx).number;
    bool isToggle = (controls.getPreset() < 24) && controls.getButtonShort(idx).toggleMode;
    if (!isToggle && type != MIDI_PC) {
        uint8_t releaseVal = (controls.getPreset() < 24) ? controls.getButtonShort(idx).minVal : 0;
        sendMidiMessage(type, number, releaseVal, channel);
        controls.getButtonShort(idx).value = releaseVal;
    }
    lastButtonReleaseTime[idx] = millis();
    if (controls.getPreset() == 24 && (idx == 3 || idx == 7)) {
        faders[idx]->updateButtonName(false);
    }
    if (controls.getPreset() == 25 && idx != 2 && idx != 3) {
        faders[idx]->updateButtonName(false);
    }
}

void sendRelativeCC(uint8_t type, uint8_t number, int delta, uint8_t channel)
{
    const int MAX_STEP = 63;
    int remaining = delta;
    while (remaining != 0) {
        int step = constrain(remaining, -MAX_STEP, MAX_STEP);
        uint8_t relValue = (uint8_t)(step & 0x7F);
        sendMidiMessage(type, number, relValue, channel);
        remaining -= step;
    }
}

void onRelativeEncoderChange(uint8_t idx, int delta)
{
    if (shiftState == SHIFT_HOLD) {
        if (delta > 0 && presetTable < 3) presetTable++;
        else if (delta < 0 && presetTable > 0) presetTable--;
        drawPresetPage();
        return;
    }

    lastControlIdx = idx;
    lastControlIsButton = false;

    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;

    controls.getEncoder(idx).lastActivity = millis();
    faders[idx]->showingValue = true;

    int estimated_display = controls.getEncoder(idx).value + delta;
    if (estimated_display < 0)
        estimated_display = 0;
    if (estimated_display > 127)
        estimated_display = 127;

    controls.getEncoder(idx).value = estimated_display;

    if (revertMode)
    {
        revertAbsoluteChanged[idx] = true;
        sendRelativeCC(type, number, delta, channel);
    }
    else if (latchPressed || latchPendingActivation)
    {
        latchRelAccum[idx] += delta;
        uint8_t relValue = (uint8_t)(delta & 0x7F);
        if (latchEncoderEventCount[idx] < MAX_LATCH_EVENTS)
        {
            latchEncoderEvents[idx][latchEncoderEventCount[idx]++] = relValue;
        }
    }
    else
    {
        sendRelativeCC(type, number, delta, channel);
    }

    if (latchPressed || latchPendingActivation || controls.getPreset() < 24)
    {
        updateFader(idx, (uint8_t)estimated_display);
        char buffer[16];
        sprintf(buffer, "%d", estimated_display);
        if (!controls.getEncoder(idx).hasWatcher)
            faders[idx]->updateTitle(buffer);
    }
}

void onAbsoluteEncoderChange(uint8_t idx, int delta)
{
    if (shiftState == SHIFT_HOLD) {
        if (delta > 0 && presetTable < 3) presetTable++;
        else if (delta < 0 && presetTable > 0) presetTable--;
        drawPresetPage();
        return;
    }

    lastControlIdx = idx;
    lastControlIsButton = false;

    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;
    controls.getEncoder(idx).lastActivity = millis();
    faders[idx]->showingValue = true;

    if ((type == MIDI_CC && controls.getEncoder(idx).hiRes) || type == MIDI_PB) {
        int newHv = (int)controls.getHiResValue(controls.getPreset(), idx) + delta;
        int minHv = (int)controls.getEncoder(idx).minVal << 7;
        int maxHv = ((int)controls.getEncoder(idx).maxVal << 7) | 0x7F;
        if (minHv >= maxHv) { minHv = 0; maxHv = 16383; }
        if (newHv < minHv) newHv = minHv;
        if (newHv > maxHv) newHv = maxHv;
        controls.setHiResValue(controls.getPreset(), idx, (uint16_t)newHv);
        uint8_t msb = (uint8_t)(newHv >> 7);
        uint8_t lsb = (uint8_t)(newHv & 0x7F);
        controls.getEncoder(idx).value = msb;
        if (revertMode) {
            revertAbsoluteChanged[idx] = true;
            if (type == MIDI_PB) sendMidiMessage(MIDI_PB, lsb, msb, channel);
            else { sendMidiMessage(MIDI_CC, number, msb, channel); sendMidiMessage(MIDI_CC, number + 32, lsb, channel); }
        } else if (latchPressed || latchPendingActivation) {
            latchAbsoluteValue[idx] = msb;
            latchAbsoluteChanged[idx] = true;
        } else {
            if (type == MIDI_PB) sendMidiMessage(MIDI_PB, lsb, msb, channel);
            else { sendMidiMessage(MIDI_CC, number, msb, channel); sendMidiMessage(MIDI_CC, number + 32, lsb, channel); }
        }
        updateFader(idx, msb);
        char buffer[16];
        sprintf(buffer, "%d", newHv);
        if (!controls.getEncoder(idx).hasWatcher)
            faders[idx]->updateTitle(buffer);
        return;
    }

    int newValue = controls.getEncoder(idx).value + delta;
    int minV = controls.getEncoder(idx).minVal;
    int maxV = controls.getEncoder(idx).maxVal;
    if (minV >= maxV) { minV = 0; maxV = 127; }
    if (newValue < minV) newValue = minV;
    if (newValue > maxV) newValue = maxV;
    controls.getEncoder(idx).value = newValue;

    if (revertMode)
    {
        revertAbsoluteChanged[idx] = true;
        sendMidiMessage(type, number, (uint8_t)newValue, channel);
    }
    else if (latchPressed || latchPendingActivation)
    {
        latchAbsoluteValue[idx] = (uint8_t)newValue;
        latchAbsoluteChanged[idx] = true;
    }
    else
    {
        sendMidiMessage(type, number, (uint8_t)newValue, channel);
    }
    updateFader(idx, (uint8_t)newValue);
    char buffer[16];
    sprintf(buffer, "%d", newValue);
    if (!controls.getEncoder(idx).hasWatcher)
        faders[idx]->updateTitle(buffer);
}

void updateFaderTitles()
{
    uint8_t preset = controls.getPreset();
    for (int i = 0; i < 8; ++i)
    {
        char buf[24];
        MidiControl& enc = controls.getEncoder(i);
        faders[i]->buttonState = false;
        faders[i]->valueOnly = (preset == 24 && (i == 2 || i == 3 || i == 6));
        if (faders[i]->valueOnly) {
            faders[i]->showingValue = true;
            faders[i]->updateTitle("---");
        } else {
            faders[i]->showingValue = false;
        }
        if (enc.controlName[0] != '\0') {
            faders[i]->setParamName(enc.controlName);
        } else if (preset == 24) {
            static const char* mixerParamNames[] = {
                "Master Vol.", "Cue Vol.", "Tempo", "Scene",
                "Volume", "Pan", "Position", "Sel. Param."
            };
            faders[i]->setParamName(mixerParamNames[i]);
        } else if (preset == 25) {
            faders[i]->setParamName("---");
        } else {
            if (enc.type == MIDI_AT)
                snprintf(buf, sizeof(buf), "AT/%d", enc.channel + 1);
            else if (enc.type == MIDI_PB)
                snprintf(buf, sizeof(buf), "PB/%d", enc.channel + 1);
            else
                snprintf(buf, sizeof(buf), "CC%d/%d", enc.number, enc.channel + 1);
            faders[i]->setParamName(buf);
        }
        if(controls.getPreset() == 25){
            static const char* buttonNames[] = {
                "Track -", "Track +", "Device On", "A/B",
                "Device -", "Device +", "Bank -", "Bank +"
            };
            snprintf(buf, sizeof(buf), buttonNames[i]);
        }
        else if(controls.getPreset() == 24){
            static const char* buttonNames[] = {
                "Metronome", "Arm", "Play/Stop", "Launch",
                "Mute", "Solo", "Arr. Loop", "-> Default"
            };
            snprintf(buf, sizeof(buf), buttonNames[i]);
        }
        else{
        MidiControl& btn = controls.getButtonShort(i);
        if (btn.controlName[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s", btn.controlName);
        } else if (btn.type == MIDI_CC) {
            snprintf(buf, sizeof(buf), "CC%d/%d", btn.number, btn.channel + 1);
        } else if (btn.type == MIDI_PC) {
            snprintf(buf, sizeof(buf), "PC%d/%d", btn.number, btn.channel + 1);
        } else {
            static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            int octave = (btn.number / 12) - 2;
            snprintf(buf, sizeof(buf), "%s%d/%d", noteNames[btn.number % 12], octave, btn.channel + 1);
        }
        }
        faders[i]->setButtonName(buf);
    }
    if (preset == 25) {
        deviceLabel[0] = '\0';
        bankLabel[0] = '\0';
        trackLabel[0] = '\0';
    } else if (preset == 24) {
        strncpy(deviceLabel, "Track", sizeof(deviceLabel));
        strncpy(bankLabel, "Global", sizeof(bankLabel));
        deviceLabelDirty = true;
        bankLabelDirty = true;
    } else {
        trackLabel[0] = '\0';
        trackLabelDirty = true;
        const char* pName = controls.getPresetName(preset);
        if (pName[0] != '\0') {
            strncpy(bankLabel, pName, sizeof(bankLabel));
        } else {
            snprintf(bankLabel, sizeof(bankLabel), "Preset %d", preset + 1);
        }
        snprintf(deviceLabel, sizeof(deviceLabel), "Preset %d", preset + 1);
        deviceLabelDirty = true;
        bankLabelDirty = true;
    }
}

void updateFaderValues()
{
    for(int i = 0; i < 8; i++) {
        uint8_t val = controls.getEncoder(i).value;
        faders[i]->setValue(val);
        if (faderLayout != LAYOUT_DYNAMIC && !controls.getEncoder(i).hasWatcher) {
            if (controls.getPreset() < 24) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", val);
                faders[i]->updateTitle(buf);
            }
        }
    }
}

void sendPresetSysEx(uint8_t preset)
{
    liveConnectedTime = millis();
    uint8_t packet[5] = {240, 111, 4, preset, 247};
    usb_midi.write(packet, 5);
}

void releaseLatchAndSend()
{
    int slewMs = slewStepsMs[latchSlewLevel];

    // Absolute encoders
    for (int i = 0; i < 8; ++i)
    {
        if (latchAbsoluteChanged[i])
        {
            ControlMidiType type = controls.getEncoder(i).type;
            if (slewMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB) {
                sendAbsoluteSlew(i, latchOriginalValue[i], latchAbsoluteValue[i], slewMs);
            } else {
                uint8_t channel = controls.getEncoder(i).channel;
                uint8_t number  = controls.getEncoder(i).number;
                sendMidiMessage(type, number, latchAbsoluteValue[i], channel);
            }
            latchAbsoluteChanged[i] = false;
        }
    }

    // Relative encoders
    if (slewMs > 0) {
        int maxAcc = 0;
        for (int i = 0; i < 8; ++i)
            if (abs(latchRelAccum[i]) > maxAcc) maxAcc = abs(latchRelAccum[i]);
        if (maxAcc > 0) {
            int totalMs = (slewMs * maxAcc + 63) / 127;
            const int stepMs = 5;
            int numSteps = max(1, totalMs / stepMs);
            int sent[8] = {0};
            for (int s = 1; s <= numSteps; ++s) {
                for (int i = 0; i < 8; ++i) {
                    if (latchRelAccum[i] == 0) continue;
                    int target = (latchRelAccum[i] * s) / numSteps;
                    int delta = target - sent[i];
                    if (delta != 0) {
                        uint8_t channel = controls.getEncoder(i).channel;
                        ControlMidiType type = controls.getEncoder(i).type;
                        uint8_t number = controls.getEncoder(i).number;
                        sendRelativeCC(type, number, delta, channel);
                        sent[i] = target;
                    }
                }
                delay(stepMs);
            }
        }
    } else {
        uint8_t maxEvents = 0;
        for (int i = 0; i < 8; ++i)
            if (latchEncoderEventCount[i] > maxEvents) maxEvents = latchEncoderEventCount[i];
        for (uint8_t eventIndex = 0; eventIndex < maxEvents; ++eventIndex) {
            for (int i = 0; i < 8; ++i) {
                if (eventIndex < latchEncoderEventCount[i]) {
                    uint8_t channel = controls.getEncoder(i).channel;
                    ControlMidiType type = controls.getEncoder(i).type;
                    uint8_t number = controls.getEncoder(i).number;
                    sendMidiMessage(type, number, latchEncoderEvents[i][eventIndex], channel);
                    delay(1);
                }
            }
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        latchEncoderEventCount[i] = 0;
        latchRelAccum[i] = 0;
    }
}

void sendRevertEvents()
{
    bool isRelative = controls.getPreset() > 23;
    int slewMs = slewStepsMs[revertSlewLevel];
    for (int i = 0; i < 8; ++i)
    {
        if (!revertAbsoluteChanged[i])
            continue;
        ControlMidiType type = controls.getEncoder(i).type;
        uint8_t channel = controls.getEncoder(i).channel;
        uint8_t number  = controls.getEncoder(i).number;

        if (isRelative)
        {
            int delta = (int)revertAbsoluteOriginal[i] - (int)controls.getEncoder(i).value;
            if (delta != 0)
                sendRelativeCC(type, number, delta, channel);
        }
        else if (slewMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB)
        {
            sendAbsoluteSlew(i, controls.getEncoder(i).value, revertAbsoluteOriginal[i], slewMs);
        }
        else
        {
            sendMidiMessage(type, number, revertAbsoluteOriginal[i], channel);
        }

        controls.getEncoder(i).value = revertAbsoluteOriginal[i];
        updateFader(i, revertAbsoluteOriginal[i]);
        revertAbsoluteChanged[i] = false;
    }
}

void setRevertModeLed(bool on)
{
    setLedBlink(0, on);
}
