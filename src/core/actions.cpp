#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"
#ifdef SEQUENCER_ENABLED
#include "../sequencer/sequencerView.h"
#include "../sequencer/lfo.h"
#endif

#define MAX_LATCH_EVENTS 256
#define SHIFT_PRESET_HOLD_MS 200
#define SHIFT_DOUBLE_TAP_WINDOW_MS 300

uint8_t presetTable = 0;
bool shiftPressed = false;
unsigned long shiftPressTime = 0;
unsigned long shiftFirstTapReleaseTime = 0;
bool shiftDoubleTapDetected = false;
bool presetModeActive = false;
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


// Pour le mode absolu : on stocke juste la dernière valeur et si elle a changé
uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

// Pour le revert en mode absolu : valeur d'origine au moment de l'entrée en revert
uint8_t revertAbsoluteOriginal[8] = {0};
bool revertAbsoluteChanged[8] = {false};

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

void onShiftPress()
{
#ifdef SEQUENCER_ENABLED
    if (sequencerActive) {
        exitSequencer();
        return;
    }
#endif
    unsigned long now = millis();
    shiftPressed = true;
    shiftPressTime = now;

    if (shiftFirstTapReleaseTime > 0 && now - shiftFirstTapReleaseTime < SHIFT_DOUBLE_TAP_WINDOW_MS) {
        shiftDoubleTapDetected = true;
        shiftFirstTapReleaseTime = 0;
    } else {
        shiftDoubleTapDetected = false;
        shiftFirstTapReleaseTime = 0;
    }

    setLed(1, true);
    sendMidiMessage(0, 110, 127, 7);

    if (revertMode)
    {
        if (controls.getPreset() > 23)
        {
            uint8_t pkt[5] = {240, 111, 0x1A, 2, 247};
            usb_midi.write(pkt, 5);
        }
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i)
            revertAbsoluteChanged[i] = false;
    }
}

void onShiftRelease()
{
    shiftPressed = false;
    setLed(1, false);
    sendMidiMessage(0, 110, 0, 7);

    if (presetModeActive) {
        presetModeActive = false;
        shiftDoubleTapDetected = false;
        showDisplay();
    } else {
        if (!shiftDoubleTapDetected)
            shiftFirstTapReleaseTime = millis();
        shiftDoubleTapDetected = false;
        for (int i = 0; i < 8; ++i)
            faders[i]->updateButtonName(controls.getButtonShort(i).value);
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

void checkShiftPreset()
{
    if (!shiftPressed || presetModeActive) return;
    if (!shiftDoubleTapDetected) return;
    if (millis() - shiftPressTime < SHIFT_PRESET_HOLD_MS) return;

    presetModeActive = true;
    display1.fillScreen(0);
    display2.fillScreen(0);
    for (int i = 0; i < 8; ++i) {
        char l1[20], l2[20] = {0};
        if (i == 6)      { strcpy(l1, "Global"); strcpy(l2, "Mode"); }
        else if (i == 7) { strcpy(l1, "Device"); strcpy(l2, "Mode"); }
        else {
            snprintf(l1, sizeof(l1), "Preset %d", i + 1);
            const char* pname = controls.getPresetName(i);
            if (pname[0] != '\0') strncpy(l2, pname, sizeof(l2) - 1);
        }
        faders[i]->drawPresetButton(l1, l2, i == controls.getPreset());
    }
    flushDisplays();
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
        for (int i = 0; i < 8; ++i)
            latchEncoderEventCount[i] = 0;
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
#ifdef SEQUENCER_ENABLED
    if (sequencerActive) return;
#endif
    lastControlIdx = idx;
    lastControlIsButton = true;
    lastInputTime = millis();
    if (screenSaverActive)
    {
        screenSaverActive = false;
        showDisplay(); // réaffiche l'UI normale
    }
    if (shiftPressed) {
#ifdef SEQUENCER_ENABLED
        if (!presetModeActive && controls.getPreset() < 24) {
            enterSequencerFor(idx);
            return;
        }
#endif
        if (presetModeActive) {
            uint8_t newPreset;
            char l1[20], l2[20] = {0};
            if (idx == 6)      { newPreset = 24; strcpy(l1, "Global"); strcpy(l2, "Mode"); }
            else if (idx == 7) { newPreset = 25; strcpy(l1, "Device"); strcpy(l2, "Mode"); }
            else {
                newPreset = presetTable * 6 + idx;
                snprintf(l1, sizeof(l1), "Preset %d", newPreset + 1);
                const char* pname = controls.getPresetName(newPreset);
                if (pname[0] != '\0') strncpy(l2, pname, sizeof(l2) - 1);
            }
            faders[idx]->drawPresetButton(l1, l2, true);
            flushDisplays();
            controls.setPreset(newPreset);
            presetModeActive = false;
            shiftDoubleTapDetected = false;
            shiftPressTime = millis();
            updateFaderTitles();
            updateFaderValues();
            showDisplay();
            sendPresetSysEx(newPreset);
        }
        return;
    }
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
#ifdef SEQUENCER_ENABLED
    if (sequencerActive) return;
#endif
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
    if (shiftPressed) {
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
#ifdef SEQUENCER_ENABLED
    // Only intercept for the LFO if this encoder's CC/Ch matches what the LFO
    // was armed with. On a different preset with different CC/Ch, send normally.
    if (lfo.isArmed(idx) && lfo.getCCNumber(idx) == number && lfo.getCCChannel(idx) == channel) {
        lfo.setValue(idx, (uint8_t)estimated_display);
    } else
#endif
    {
        if (revertMode)
        {
            revertAbsoluteChanged[idx] = true;
            sendRelativeCC(type, number, delta, channel);
        }
        else if (!latchPressed)
        {
            sendRelativeCC(type, number, delta, channel);
        }
        else
        {
            uint8_t relValue = (uint8_t)(delta & 0x7F);
            if (latchEncoderEventCount[idx] < MAX_LATCH_EVENTS)
            {
                latchEncoderEvents[idx][latchEncoderEventCount[idx]++] = relValue;
            }
        }
    }

    if (latchPressed || controls.getPreset() < 24)
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
    if (shiftPressed) {
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
        } else if (!latchPressed) {
            if (type == MIDI_PB) sendMidiMessage(MIDI_PB, lsb, msb, channel);
            else { sendMidiMessage(MIDI_CC, number, msb, channel); sendMidiMessage(MIDI_CC, number + 32, lsb, channel); }
        } else {
            latchAbsoluteValue[idx] = msb;
            latchAbsoluteChanged[idx] = true;
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
#ifdef SEQUENCER_ENABLED
    if (lfo.isArmed(idx) && lfo.getCCNumber(idx) == number && lfo.getCCChannel(idx) == channel) {
        lfo.setValue(idx, (uint8_t)newValue);
    } else
#endif
    {
        if (revertMode)
        {
            revertAbsoluteChanged[idx] = true;
            sendMidiMessage(type, number, (uint8_t)newValue, channel);
        }
        else if (!latchPressed)
        {
            sendMidiMessage(type, number, (uint8_t)newValue, channel);
        }
        else
        {
            latchAbsoluteValue[idx] = (uint8_t)newValue;
            latchAbsoluteChanged[idx] = true;
        }
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
    // Envoyer les dernières valeurs absolues stockées
    for (int i = 0; i < 8; ++i)
    {
        if (latchAbsoluteChanged[i])
        {
            uint8_t channel = controls.getEncoder(i).channel;
            ControlMidiType type = controls.getEncoder(i).type;
            uint8_t number = controls.getEncoder(i).number;
            sendMidiMessage(type, number, latchAbsoluteValue[i], channel);
            latchAbsoluteChanged[i] = false;
        }
    }

    // Trouver le nombre maximum d'événements parmi tous les encodeurs (mode relatif)
    uint8_t maxEvents = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (latchEncoderEventCount[i] > maxEvents)
        {
            maxEvents = latchEncoderEventCount[i];
        }
    }

    // Envoyer les événements de manière entrelacée (mode relatif)
    for (uint8_t eventIndex = 0; eventIndex < maxEvents; ++eventIndex)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (eventIndex < latchEncoderEventCount[i])
            {
                uint8_t channel = controls.getEncoder(i).channel;
                ControlMidiType type = controls.getEncoder(i).type;
                uint8_t number = controls.getEncoder(i).number;

                sendMidiMessage(type, number, latchEncoderEvents[i][eventIndex], channel);
                delay(1);
            }
        }
    }

    // Réinitialiser les compteurs
    for (int i = 0; i < 8; ++i)
    {
        latchEncoderEventCount[i] = 0;
    }
}

void sendRevertEvents()
{
    bool isRelative = controls.getPreset() > 23;
    for (int i = 0; i < 8; ++i)
    {
        if (!revertAbsoluteChanged[i])
            continue;
        uint8_t channel = controls.getEncoder(i).channel;
        ControlMidiType type = controls.getEncoder(i).type;
        uint8_t number = controls.getEncoder(i).number;

        if (isRelative)
        {
            int delta = (int)revertAbsoluteOriginal[i] - (int)controls.getEncoder(i).value;
            if (delta != 0)
                sendRelativeCC(type, number, delta, channel);
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