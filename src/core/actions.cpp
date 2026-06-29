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

// Pour le mode absolu : on stocke juste la dernière valeur et si elle a changé
uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

void onShiftPress()
{
    shiftPressed = true;
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
    sendMidiMessage(0, 110, 0, 7);    
    for (int i = 0; i < 8; ++i)
    {
    faders[i]->updateButtonName(controls.getButtonShort(i).value);
    }
}

void onLatchPress()
{
    for (int i = 0; i < 8; ++i)
    {
        latchEncoderEventCount[i] = 0;
    }
    latchPressed = true;
    sendMidiMessage(0, 111, 127, 7);

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
    latchPressed = false;
    releaseLatchAndSend();
    sendMidiMessage(0, 111, 0, 7);
}

void onButtonShortPress(uint8_t idx)
{
}

void onButtonPressed(uint8_t idx)
{
    lastInputTime = millis();
    if (screenSaverActive)
    {
        screenSaverActive = false;
        showDisplay(); // réaffiche l'UI normale
    }
    if (shiftPressed)
    {
        // blinkLedBlue(idx, 2);

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

void onRelativeEncoderChange(uint8_t idx, int delta)
{

    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;

    controls.getEncoder(idx).lastActivity = millis();

    uint8_t relValue = (uint8_t)(delta & 0x7F);
    // Pour Relative Binary Offset (Ableton)
    // uint8_t relValue = 0x40 + constrain(delta, -63, 63);
    // Calculer et mettre à jour la valeur cumulative
    int estimated_display = controls.getEncoder(idx).value + delta;
    if (estimated_display < 0)
        estimated_display = 0;
    if (estimated_display > 127)
        estimated_display = 127;

    // Mettre à jour la valeur stockée dans le contrôle
    controls.getEncoder(idx).value = estimated_display;

    if (revertMode)
    {
        if (revertEncoderEventCount[idx] < MAX_LATCH_EVENTS)
        {
            revertEncoderEvents[idx][revertEncoderEventCount[idx]++] = relValue;
        }
        sendMidiMessage(type, number, relValue, channel);
    }
    else if (!latchPressed)
    {
        sendMidiMessage(type, number, relValue, channel);
    }
    else
    {
        if (latchEncoderEventCount[idx] < MAX_LATCH_EVENTS)
        {
            latchEncoderEvents[idx][latchEncoderEventCount[idx]++] = relValue;
        }
    }

    // Mettre à jour le fader avec la valeur cumulative
    if (latchPressed || controls.getPreset() < 6)
    {
        updateFader(idx, (uint8_t)estimated_display);
        char buffer[16];
        sprintf(buffer, "%d", estimated_display); // pour un int
        faders[idx]->updateTitle(buffer);
    }
}

void onAbsoluteEncoderChange(uint8_t idx, int delta)
{
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number = controls.getEncoder(idx).number;
    controls.getEncoder(idx).lastActivity = millis();
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
        // En mode latch, on stocke juste la dernière valeur
        latchAbsoluteValue[idx] = (uint8_t)newValue;
        latchAbsoluteChanged[idx] = true;
    }
    
    updateFader(idx, (uint8_t)newValue);
    char buffer[16];
    sprintf(buffer, "%d", newValue);
    faders[idx]->updateTitle(buffer);
}

void updateFaderTitles()
{
    for (int i = 0; i < 8; ++i)
    {
        char buf[24];
        int number = controls.getEncoder(i).number;
        int channel = controls.getEncoder(i).channel;
        snprintf(buf, sizeof(buf), "CC%d/%d", number, channel + 1);
        faders[i]->setParamName(buf);
        if(controls.getPreset() == 7){
                    static const char* buttonNames[] = {
            "Track -", "Track +", "Hotswap", "A/B",
            "Device -", "Device +", "Bank -", "Bank +"
        };
        snprintf(buf, sizeof(buf), buttonNames[i]);
        }        
        else if(controls.getPreset() == 6){
                    static const char* buttonNames[] = {
            "Metronome", "Arr. Rec", "Play/Stop", "Capture",
            "Mute", "Solo", "Arr. Loop", "-> Default"
        };
        snprintf(buf, sizeof(buf), buttonNames[i]);
        }
        else{
        ControlMidiType type = controls.getButtonShort(i).type;
        number = controls.getButtonShort(i).number;
        channel = controls.getButtonShort(i).channel;
        snprintf(buf, sizeof(buf), (type==MIDI_CC) ? "CC%d/%d" : "Note%d/%d", number, channel + 1);
        }
        faders[i]->setButtonName(buf);
    }
}

void updateFaderValues()
{
    for(int i = 0; i < 8; i++) {
        uint8_t val = controls.getEncoder(i).value;
        faders[i]->setValue(val);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", val);
        faders[i]->updateTitle(buf);
    }
}

void sendPresetSysEx(uint8_t preset)
{
    uint8_t packet[5] = {240, 111, 4, preset, 247};
    usb_midi.writePacket(packet);
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
        showLed(8, 0, 0, 0);
    }
}