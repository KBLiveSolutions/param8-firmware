#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "../view/display.h"
#include "../view/leds.h"

#define MAX_LATCH_EVENTS 256
bool shiftPressed = false;
bool latchPressed = false;
uint8_t latchEncoderEvents[8][MAX_LATCH_EVENTS] = {{0}};
uint8_t latchEncoderEventCount[8] = {0};
bool revertMode = false;
uint8_t revertEncoderEvents[8][MAX_LATCH_EVENTS] = {{0}};
uint8_t revertEncoderEventCount[8] = {0};
unsigned long lastInputTime = 0;
uint8_t lastControlIdx = 0;
bool lastControlIsButton = false;
unsigned long lastLatchPressTime = 0;
bool latchPendingActivation = false;
bool latchHeld = false;

// Pour le mode absolu : on stocke juste la dernière valeur et si elle a changé
uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

void onShiftPress()
{
    shiftPressed = true;
    setLed(1, true);
    sendMidiMessage(0, 110, 127, 7);

    if (revertMode)
    {
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i)
        {
            revertEncoderEventCount[i] = 0;
        }
    }
    for (int i = 0; i < 8; ++i)
    {

        char buf[24];
        static const char* buttonNames[] = {
            "Preset 1", "Preset 2", "Preset 3", "Preset 4",
            "Preset 5", "Preset 6", "Global", "Device"
        };
        snprintf(buf, sizeof(buf), buttonNames[i]);
        faders[i]->drawButtonName(buf, i==controls.getPreset());
    }
    //updateDisplayBox("left", "LEFT", staticOverlay == 1);
    //updateDisplayBox("right", "RIGHT", staticOverlay == 1);
}

void onShiftRelease()
{
    shiftPressed = false;
    setLed(1, false);
    sendMidiMessage(0, 110, 0, 7);    
    for (int i = 0; i < 8; ++i)
    {
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
        sendNameRequest(lastControlIdx, lastControlIsButton ? 1 : 0);
        lastLatchPressTime = 0;
        return;
    }
    lastLatchPressTime = now;
    latchPendingActivation = true;

    if (revertMode && !shiftPressed)
    {
        revertMode = false;
        setRevertModeLed(false);
        for (int i = 0; i < 8; ++i)
        {
            revertEncoderEventCount[i] = 0;
        }
        return;
    }

    if (shiftPressed)
    {
        if (!revertMode)
        {
            revertMode = true;
            for (int i = 0; i < 8; ++i)
            {
                revertEncoderEventCount[i] = 0;
            }
            setRevertModeLed(true);
        }
        else
        {
            sendRevertEvents();
            revertMode = false;
            setRevertModeLed(false);
            for (int i = 0; i < 8; ++i)
            {
                revertEncoderEventCount[i] = 0;
            }
        }
    }
}

void onLatchRelease()
{
    latchHeld = false;
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
        showDisplay(); // réaffiche l'UI normale
    }
    if (shiftPressed)
    {
        char buf[24];
        static const char* buttonNames[] = {
            "Preset 1", "Preset 2", "Preset 3", "Preset 4",
            "Preset 5", "Preset 6", "Global", "Device"
        };
        snprintf(buf, sizeof(buf), buttonNames[idx]);
        faders[idx]->drawButtonName(buf,true);
        updateDisplayBox("left", buf);
        updateDisplayBox("right", buf);

        controls.setPreset(idx);
        updateFaderTitles();
        updateFaderValues();
        sendPresetSysEx(idx);
        return;
    }
    else
    {
    Serial.println(controls.getButtonShort(idx).toggleMode);
        uint8_t _value = 127;
        if (controls.getButtonShort(idx).toggleMode) _value = (controls.getButtonShort(idx).value == 0) ? 127 : 0;
        controls.getButtonShort(idx).value = _value;
        uint8_t channel = controls.getButtonShort(idx).channel;
        ControlMidiType type = controls.getButtonShort(idx).type;
        uint8_t number = controls.getButtonShort(idx).number;
        // uint8_t value = controls.getButtonShort(idx).value;
        sendMidiMessage(type, number, _value, channel);
    }
}

void onButtonReleased(uint8_t idx)
{
    if (controls.getButtonShort(idx).toggleMode)
    return;
    uint8_t channel = controls.getButtonShort(idx).channel;
    ControlMidiType type = controls.getButtonShort(idx).type;
    uint8_t number = controls.getButtonShort(idx).number;
    sendMidiMessage(type, number, 0, channel);
    controls.getButtonShort(idx).value = 0;
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
        uint8_t relValue = (uint8_t)(delta & 0x7F);
        if (revertEncoderEventCount[idx] < MAX_LATCH_EVENTS)
        {
            revertEncoderEvents[idx][revertEncoderEventCount[idx]++] = relValue;
        }
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

    if (latchPressed || controls.getPreset() < 6)
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
    lastControlIdx = idx;
    lastControlIsButton = false;

    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;
    controls.getEncoder(idx).lastActivity = millis();
    faders[idx]->showingValue = true;
    int newValue = controls.getEncoder(idx).value + delta;
    if (newValue < 0)
        newValue = 0;
    if (newValue > 127)
        newValue = 127;
    controls.getEncoder(idx).value = newValue;
    
    if (!latchPressed)
    {
        sendMidiMessage(type, number, (uint8_t)newValue, channel);
    }
    else
    {
        latchAbsoluteValue[idx] = (uint8_t)newValue;
        latchAbsoluteChanged[idx] = true;
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
        faders[i]->valueOnly = (preset == 6 && (i == 2 || i == 3 || i == 6));
        if (faders[i]->valueOnly) {
            faders[i]->showingValue = true;
            faders[i]->updateTitle("---");
        }
        if (enc.controlName[0] != '\0') {
            faders[i]->setParamName(enc.controlName);
        } else if (preset == 6) {
            static const char* mixerParamNames[] = {
                "Master Vol.", "Cue Vol.", "Tempo", "Scene",
                "Volume", "Pan", "Position", "Sel. Param."
            };
            faders[i]->setParamName(mixerParamNames[i]);
        } else if (preset == 7) {
            faders[i]->setParamName("---");
        } else {
            snprintf(buf, sizeof(buf), "CC%d/%d", enc.number, enc.channel + 1);
            faders[i]->setParamName(buf);
        }
        if(controls.getPreset() == 7){
            static const char* buttonNames[] = {
                "Track -", "Track +", "Hotswap", "A/B",
                "Device -", "Device +", "Bank -", "Bank +"
            };
            snprintf(buf, sizeof(buf), buttonNames[i]);
        }
        else if(controls.getPreset() == 6){
            static const char* buttonNames[] = {
                "Metronome", "Arr. Rec", "Play/Stop", "Launch",
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
        } else {
            static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            int octave = (btn.number / 12) - 2;
            snprintf(buf, sizeof(buf), "%s%d/%d", noteNames[btn.number % 12], octave, btn.channel + 1);
        }
        }
        faders[i]->setButtonName(buf);
    }
    if (preset == 7) {
        deviceLabel[0] = '\0';
        bankLabel[0] = '\0';
    } else if (preset == 6) {
        strncpy(deviceLabel, "Track", sizeof(deviceLabel));
        strncpy(bankLabel, "Global", sizeof(bankLabel));
        deviceLabelDirty = true;
        bankLabelDirty = true;
    } else {
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
            if (controls.getPreset() < 6) {
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
    for (int i = 0; i < 8; ++i)
    {
        uint8_t channel = controls.getEncoder(i).channel;
        ControlMidiType type = controls.getEncoder(i).type;
        uint8_t number = controls.getEncoder(i).number;
        for (uint8_t j = 0; j < revertEncoderEventCount[i]; ++j)
        {
            uint8_t val = revertEncoderEvents[i][j];
            uint8_t inv = 0;
            if (val == 0)
                inv = 0;
            else if (val <= 0x3F)
                inv = (0x80 - val) & 0x7F;
            else
                inv = (0x80 - val) & 0x7F;
            sendMidiMessage(type, number, inv, channel);
        }
    }
}

void setRevertModeLed(bool on)
{
    if (!on)
    {
        setLed(1, false);
    }
}