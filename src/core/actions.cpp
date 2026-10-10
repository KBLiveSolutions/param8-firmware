#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"

#define MAX_LATCH_EVENTS 256
#define SHIFT_HOLD_MS     300   // hold to show preset page
#define SHIFT_DTAP_MS     400   // max gap (1st release → 2nd press) for double-tap
#define SHIFT_DTHOLD_MS   500   // hold on 2nd press to trigger double-tap-hold

uint8_t presetTable = 0;
bool shiftPressed = false;
bool settingsModeActive = false;
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

// Pour le mode absolu : on stocke juste la dernière valeur et si elle a changé
uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

// Pour le mode relatif : delta accumulé pendant le latch
static int latchRelAccum[8] = {0};

// Valeurs au moment de l'activation du latch (pour annulation)
static uint8_t latchOriginalValue[8] = {0};

// État des widgets 0–3 sauvegardé avant d'entrer en mode settings
struct FaderSaveState {
    char paramName[20], title[20], buttonName[20], buttonText[20];
    int  value;
    bool disabled, dimmed, showingValue, valueOnly;
};
static FaderSaveState settingsSaved[4];  // faders 0–3

// Screensaver level index (0–7) pour le menu settings
static const unsigned long ssStepsSec[] = {0, 60, 120, 300, 600, 1800, 3600, 7200};
static const char* const   ssStepLabels[] = {"Off", "1 min", "2 min", "5 min", "10 min", "30 min", "1 h", "2 h"};
#define SS_STEPS 8
static uint8_t screensaverLevel = 3;  // 5 min par défaut

// Slew level index (0–5) pour latch et revert
static const int   slewStepsMs[] = {0, 100, 200, 500, 1000, 2000};
static const char* const slewStepLabels[] = {"Off", "100ms", "200ms", "500ms", "1 s", "2 s"};
#define SLEW_STEPS 6
uint8_t latchSlewLevel  = 0;
uint8_t revertSlewLevel = 0;
static bool settingsPendingSave = false;

// Pour le revert en mode absolu : valeur d'origine au moment de l'entrée en revert
uint8_t revertAbsoluteOriginal[8] = {0};
bool revertAbsoluteChanged[8] = {false};

// Envoie fromVal→toVal en glide (5 ms/step, durée proportionnelle au delta)
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
    // Screensaver level
    unsigned long sec = screenSaverDelay / 1000UL;
    screensaverLevel = 0;
    for (int i = SS_STEPS - 1; i > 0; --i)
        if (sec >= ssStepsSec[i]) { screensaverLevel = i; break; }
    // Slew levels
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

static void enterSettingsMode() {
    for (int i = 0; i < 4; ++i) {
        strncpy(settingsSaved[i].paramName,  faders[i]->paramName,  20);
        strncpy(settingsSaved[i].title,      faders[i]->title,      20);
        strncpy(settingsSaved[i].buttonName, faders[i]->buttonName, 20);
        strncpy(settingsSaved[i].buttonText, faders[i]->buttonText, 20);
        settingsSaved[i].value        = faders[i]->value;
        settingsSaved[i].disabled     = faders[i]->disabled;
        settingsSaved[i].dimmed       = faders[i]->dimmed;
        settingsSaved[i].showingValue = faders[i]->showingValue;
        settingsSaved[i].valueOnly    = faders[i]->valueOnly;
    }
    unsigned long sec = screenSaverDelay / 1000UL;
    screensaverLevel = 0;
    for (int i = SS_STEPS - 1; i > 0; --i) {
        if (sec >= ssStepsSec[i]) { screensaverLevel = i; break; }
    }
    settingsModeActive = true;
    setLed(1, true);
}

static void drawSettingsPage() {
    display1.fillScreen(0);
    display2.fillScreen(0);
    const char* layoutName = (faderLayout == LAYOUT_DYNAMIC) ? "Dynamic" : "Compact";
    faders[0]->disabled   = false;
    faders[0]->dimmed     = false;
    faders[0]->valueOnly  = false;
    faders[0]->showingValue = false;
    faders[0]->setParamName("Brightness");
    faders[0]->setValue(brightnessLevel * 63);
    faders[0]->setButtonName(layoutName);
    faders[0]->draw();
    faders[1]->disabled   = false;
    faders[1]->dimmed     = false;
    faders[1]->valueOnly  = false;
    faders[1]->showingValue = false;
    faders[1]->setParamName("Screensaver");
    faders[1]->setValue(screensaverLevel * 18);
    faders[1]->setButtonName(ssStepLabels[screensaverLevel]);
    faders[1]->draw();
    faders[2]->disabled   = false;
    faders[2]->dimmed     = false;
    faders[2]->valueOnly  = false;
    faders[2]->showingValue = false;
    faders[2]->setParamName("Latch Slew");
    faders[2]->value = latchSlewLevel * 25;
    strncpy(faders[2]->buttonName, slewStepLabels[latchSlewLevel], 20);
    strncpy(faders[2]->buttonText, slewStepLabels[latchSlewLevel], 20);
    faders[2]->draw();
    faders[3]->disabled   = false;
    faders[3]->dimmed     = false;
    faders[3]->valueOnly  = false;
    faders[3]->showingValue = false;
    faders[3]->setParamName("Revert Slew");
    faders[3]->value = revertSlewLevel * 25;
    strncpy(faders[3]->buttonName, slewStepLabels[revertSlewLevel], 20);
    strncpy(faders[3]->buttonText, slewStepLabels[revertSlewLevel], 20);
    faders[3]->draw();
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

    if (settingsModeActive) {
        settingsModeActive = false;
        setLed(1, false);
        for (int i = 0; i < 4; ++i) {
            strncpy(faders[i]->paramName,  settingsSaved[i].paramName,  20);
            strncpy(faders[i]->title,      settingsSaved[i].title,      20);
            strncpy(faders[i]->buttonName, settingsSaved[i].buttonName, 20);
            strncpy(faders[i]->buttonText, settingsSaved[i].buttonText, 20);
            faders[i]->value        = settingsSaved[i].value;
            faders[i]->disabled     = settingsSaved[i].disabled;
            faders[i]->dimmed       = settingsSaved[i].dimmed;
            faders[i]->showingValue = settingsSaved[i].showingValue;
            faders[i]->valueOnly    = settingsSaved[i].valueOnly;
        }
        json.getDoc()["screensaver"] = (int)ssStepsSec[screensaverLevel];
        json.getDoc()["brightness"]  = (int)brightnessLevel;
        json.getDoc()["latchSlew"]   = (int)latchSlewLevel;
        json.getDoc()["revertSlew"]  = (int)revertSlewLevel;
        settingsPendingSave = true;
        showDisplay();
        shiftState = SHIFT_IDLE;
        shiftPressed = true;
        return;
    }

    switch (shiftState) {
    case SHIFT_IDLE:
        startFirstPress(now);
        break;

    case SHIFT_UP1:
        if (now - shiftStateTime < SHIFT_DTAP_MS) {
            // Double-tap: 2nd press within window
            shiftState = SHIFT_DOWN2;
            shiftStateTime = now;
            shiftPressed = true;  // allow buttons.cpp to detect the release
        } else {
            // Too slow — new first press
            startFirstPress(now);
        }
        break;

    case SHIFT_DOWN1:
        // 2nd press before 1st release (fast double-tap)
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
        break;  // ignore
    }
}

void onShiftRelease()
{
    unsigned long now = millis();

    if (settingsModeActive) {
        shiftState = SHIFT_IDLE;
        return;
    }

    switch (shiftState) {
    case SHIFT_DOWN1:
        // Quick release before SHIFT_HOLD_MS → potential double-tap
        shiftPressed = false;
        setLed(1, false);
        sendMidiMessage(0, 110, 0, 7);
        shiftState = SHIFT_UP1;
        shiftStateTime = now;
        break;

    case SHIFT_HOLD:
        // Long hold release
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
        if (now - shiftStateTime < SHIFT_DTHOLD_MS) {
            // Quick release of 2nd press → double-tap confirmed → settings
            enterSettingsMode();
            drawSettingsPage();
        }
        // else: held ≥ SHIFT_DTHOLD_MS → double-tap-hold (timer already fired)
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
    if (settingsPendingSave) {
        settingsPendingSave = false;
        json.save();
    }
    unsigned long now = millis();
    switch (shiftState) {
    case SHIFT_DOWN1:
        if (now - shiftStateTime >= SHIFT_HOLD_MS) {
            shiftState = SHIFT_HOLD;
            drawPresetPage();
        }
        break;
    case SHIFT_UP1:
        if (now - shiftStateTime >= SHIFT_DTAP_MS) {
            // Single tap confirmed
            shiftState = SHIFT_IDLE;
            showDisplay();
            for (int i = 0; i < 8; ++i)
                faders[i]->updateButtonName(faders[i]->buttonState);
            flushDisplays();
        }
        break;
    case SHIFT_DOWN2:
        if (now - shiftStateTime >= SHIFT_DTHOLD_MS) {
            // Double-tap-hold (placeholder — step sequencer branch)
            shiftPressed = false;
            shiftState = SHIFT_IDLE;
        }
        break;
    default:
        break;
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
        for (int i = 0; i < 8; ++i) {
            latchEncoderEventCount[i] = 0;
            latchRelAccum[i] = 0;
            latchOriginalValue[i] = controls.getEncoder(i).value;
        }
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
    lastControlIdx = idx;
    lastControlIsButton = true;
    lastInputTime = millis();
    if (screenSaverActive)
    {
        screenSaverActive = false;
        if (settingsModeActive) drawSettingsPage();
        else showDisplay();
    }
    if (settingsModeActive) {
        if (idx == 0) {
            faderLayout = (faderLayout == LAYOUT_DYNAMIC) ? LAYOUT_COMPACT : LAYOUT_DYNAMIC;
            drawSettingsPage();
        }
        return;
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
    if (settingsModeActive) return;
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

static void handleSettingsBrightness(int delta) {
    int newLevel = (int)brightnessLevel + (delta > 0 ? 1 : -1);
    brightnessLevel = (uint8_t)constrain(newLevel, 0, 2);
    setBrightness(brightnessLevel);
    faders[0]->value = brightnessLevel * 63;
    faders[0]->draw();
    flushDisplays();
}

static void handleSettingsScreensaver(int delta) {
    int newLevel = (int)screensaverLevel + (delta > 0 ? 1 : -1);
    screensaverLevel = (uint8_t)constrain(newLevel, 0, SS_STEPS - 1);
    screenSaverDelay = ssStepsSec[screensaverLevel] * 1000UL;
    faders[1]->value = screensaverLevel * 18;
    strncpy(faders[1]->buttonName, ssStepLabels[screensaverLevel], 20);
    strncpy(faders[1]->buttonText, ssStepLabels[screensaverLevel], 20);
    faders[1]->draw();
    flushDisplays();
}

static void handleSettingsLatchSlew(int delta) {
    int newLevel = (int)latchSlewLevel + (delta > 0 ? 1 : -1);
    latchSlewLevel = (uint8_t)constrain(newLevel, 0, SLEW_STEPS - 1);
    faders[2]->value = latchSlewLevel * 25;
    strncpy(faders[2]->buttonName, slewStepLabels[latchSlewLevel], 20);
    strncpy(faders[2]->buttonText, slewStepLabels[latchSlewLevel], 20);
    faders[2]->draw();
    flushDisplays();
}

static void handleSettingsRevertSlew(int delta) {
    int newLevel = (int)revertSlewLevel + (delta > 0 ? 1 : -1);
    revertSlewLevel = (uint8_t)constrain(newLevel, 0, SLEW_STEPS - 1);
    faders[3]->value = revertSlewLevel * 25;
    strncpy(faders[3]->buttonName, slewStepLabels[revertSlewLevel], 20);
    strncpy(faders[3]->buttonText, slewStepLabels[revertSlewLevel], 20);
    faders[3]->draw();
    flushDisplays();
}

void onRelativeEncoderChange(uint8_t idx, int delta)
{
    if (settingsModeActive) {
        if      (idx == 0) handleSettingsBrightness(delta);
        else if (idx == 1) handleSettingsScreensaver(delta);
        else if (idx == 2) handleSettingsLatchSlew(delta);
        else if (idx == 3) handleSettingsRevertSlew(delta);
        return;
    }
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
    else if (!latchPressed)
    {
        sendRelativeCC(type, number, delta, channel);
    }
    else
    {
        latchRelAccum[idx] += delta;
        uint8_t relValue = (uint8_t)(delta & 0x7F);
        if (latchEncoderEventCount[idx] < MAX_LATCH_EVENTS)
        {
            latchEncoderEvents[idx][latchEncoderEventCount[idx]++] = relValue;
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
    if (settingsModeActive) {
        if      (idx == 0) handleSettingsBrightness(delta);
        else if (idx == 1) handleSettingsScreensaver(delta);
        else if (idx == 2) handleSettingsLatchSlew(delta);
        else if (idx == 3) handleSettingsRevertSlew(delta);
        return;
    }
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
    // Envoyer les dernières valeurs absolues stockées
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

    // Mode relatif
    if (slewMs > 0) {
        // Étaler le delta accumulé sur slewMs (proportionnel comme le slew absolu)
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
        // Rejouer les événements individuels de manière entrelacée
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