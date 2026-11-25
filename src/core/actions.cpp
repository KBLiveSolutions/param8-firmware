#include "../midi/midi.h"
#include "actions.h"
#include "controls.h"
#include "jsonManager.h"
#include "../view/display.h"
#include "../view/leds.h"
#include "../usb/serial_editor.h"

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
static bool presetJustSelected = false;
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
static bool pendingPresetSave = false;
static bool pendingFullRedraw = false;
static unsigned long lastPresetNavTime = 0;
static bool pendingSmoothSave = false;
static unsigned long pendingSmoothSaveTime = 0;
static uint8_t navPresetCursor = 0;

uint8_t latchAbsoluteValue[8] = {0};
bool latchAbsoluteChanged[8] = {false};

static int latchRelAccum[8] = {0};
static uint8_t latchOriginalValue[8] = {0};

struct FaderSaveState {
    char paramName[20], title[20], buttonName[20], buttonText[20];
    int  value;
    bool disabled, dimmed, showingValue, valueOnly;
    bool isQuantized;
    uint8_t numQuantizedItems;
};
static FaderSaveState settingsSaved[6];

static const unsigned long ssStepsSec[] = {0, 60, 120, 300, 600, 1800, 3600, 7200};
static const char* const   ssStepLabels[] = {"Off", "1 min", "2 min", "5 min", "10 min", "30 min", "1 h", "2 h"};
#define SS_STEPS 8
static uint8_t screensaverLevel = 3;

static const char* const syncStepLabels[] = {"1/16","1/8","1/4","1/2","1 bar","2 bar","4 bar","8 bar"};
static const float       syncStepQuarters[] = {0.25f,0.5f,1.0f,2.0f,4.0f,8.0f,16.0f,32.0f};
#define SYNC_STEPS 8

uint32_t liveBpmCents = 12000; // default 120.00 BPM

static int smoothLevelToMs(uint8_t level) {
    if (level == 0) return 0;
    return (int)roundf(100.0f * powf(590.0f, (level - 1) / 126.0f));
}
static int syncSmoothToMs(uint8_t step) {
    if (liveBpmCents == 0) return 0;
    float msPerQuarter = 60000.0f / (liveBpmCents / 100.0f);
    return (int)(msPerQuarter * syncStepQuarters[step]);
}
static void formatSmoothLabel(uint8_t level, bool syncMode, uint8_t syncStep, char* buf, size_t len) {
    if (syncMode) { strncpy(buf, syncStepLabels[syncStep], len); return; }
    if (level == 0) { strncpy(buf, "Off", len); return; }
    int ms = smoothLevelToMs(level);
    if (ms < 1000) snprintf(buf, len, "%dms", ms);
    else           snprintf(buf, len, "%.1fs", ms / 1000.0f);
}
uint8_t latchSmoothLevel  = 0;
uint8_t revertSmoothLevel = 0;
bool    latchSmoothSync   = false;
bool    revertSmoothSync  = false;
uint8_t latchSmoothSyncStep  = 2; // 1/4 note default
uint8_t revertSmoothSyncStep = 2;
static bool settingsPendingSave = false;

uint8_t revertAbsoluteOriginal[8] = {0};
bool revertAbsoluteChanged[8] = {false};

struct SmoothOp {
    bool     active   = false;
    int      fromVal  = 0;
    int      toVal    = 0;
    int      lastSent = 0;
    unsigned long startTime = 0;
    unsigned long totalMs   = 0;
    ControlMidiType type;
    uint8_t  number  = 0;
    uint8_t  channel = 0;
};
static SmoothOp smoothOps[8];

struct RelSmoothOp {
    bool active = false;
    int  target[8] = {0};
    int  sent[8]   = {0};
    unsigned long startTime = 0;
    unsigned long totalMs   = 0;
};
static RelSmoothOp relSmooth;
static RelSmoothOp revertRelSmooth;

bool deviceLocked = false;

static void startAbsoluteSmooth(uint8_t idx, int fromVal, int toVal, int smoothMs) {
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number  = controls.getEncoder(idx).number;
    if (fromVal == toVal || smoothMs == 0) { sendMidiMessage(type, number, (uint8_t)toVal, channel); return; }
    unsigned long totalMs = (unsigned long)smoothMs;
    smoothOps[idx].active    = true;
    smoothOps[idx].fromVal   = fromVal;
    smoothOps[idx].toVal     = toVal;
    smoothOps[idx].lastSent  = fromVal;
    smoothOps[idx].startTime = millis();
    smoothOps[idx].totalMs   = totalMs;
    smoothOps[idx].type      = type;
    smoothOps[idx].number    = number;
    smoothOps[idx].channel   = channel;
}

// kept only to avoid touching the old dead-code path below:
static void sendAbsoluteSmooth(uint8_t idx, int fromVal, int toVal, int smoothMs) {
    uint8_t  channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t  number  = controls.getEncoder(idx).number;
    int range = abs(toVal - fromVal);
    if (range == 0) { sendMidiMessage(type, number, (uint8_t)toVal, channel); return; }
    int totalMs = (smoothMs * range + 63) / 127;
    const int stepMs = 5;
    int steps = max(1, totalMs / stepMs);
    for (int s = 1; s <= steps; s++) {
        int v = fromVal + ((toVal - fromVal) * s) / steps;
        sendMidiMessage(type, number, (uint8_t)constrain(v, 0, 127), channel);
        delay(stepMs);
    }
}

bool isSelectingPreset() {
    return shiftState == SHIFT_HOLD;
}

void sendRelativeCC(uint8_t type, uint8_t number, int delta, uint8_t channel, uint8_t cable = 255);


void checkSmoothProgress() {
    unsigned long now = millis();

    for (int i = 0; i < 8; ++i) {
        if (!smoothOps[i].active) continue;
        unsigned long elapsed = now - smoothOps[i].startTime;
        bool done = elapsed >= smoothOps[i].totalMs;
        int newVal = done
            ? smoothOps[i].toVal
            : smoothOps[i].fromVal + (int)((long)(smoothOps[i].toVal - smoothOps[i].fromVal) * (long)elapsed / (long)smoothOps[i].totalMs);
        newVal = constrain(newVal, 0, 127);
        if (newVal != smoothOps[i].lastSent) {
            sendMidiMessage(smoothOps[i].type, smoothOps[i].number, (uint8_t)newVal, smoothOps[i].channel);
            controls.getEncoder(i).value = (uint8_t)newVal;
            updateFader(i, newVal);
            smoothOps[i].lastSent = newVal;
        }
        if (done) smoothOps[i].active = false;
    }

    if (relSmooth.active) {
        unsigned long elapsed = now - relSmooth.startTime;
        bool done = elapsed >= relSmooth.totalMs;
        for (int i = 0; i < 8; ++i) {
            if (relSmooth.target[i] == 0) continue;
            int shouldHaveSent = done
                ? relSmooth.target[i]
                : (int)((long)relSmooth.target[i] * (long)elapsed / (long)relSmooth.totalMs);
            int delta = shouldHaveSent - relSmooth.sent[i];
            if (delta != 0) {
                if (controls.getEncoder(i).isFunction)
                    sendRelativeCC(MIDI_CC, controls.getEncoder(i).fnId, delta, 7, 0);
                else {
                    uint8_t ch  = controls.getEncoder(i).channel;
                    ControlMidiType t = controls.getEncoder(i).type;
                    uint8_t num = controls.getEncoder(i).number;
                    sendRelativeCC(t, num, delta, ch);
                }
                relSmooth.sent[i] = shouldHaveSent;
            }
        }
        if (done) relSmooth.active = false;
    }

    if (revertRelSmooth.active) {
        unsigned long elapsed = now - revertRelSmooth.startTime;
        bool done = elapsed >= revertRelSmooth.totalMs;
        for (int i = 0; i < 8; ++i) {
            if (revertRelSmooth.target[i] == 0) continue;
            int shouldHaveSent = done
                ? revertRelSmooth.target[i]
                : (int)((long)revertRelSmooth.target[i] * (long)elapsed / (long)revertRelSmooth.totalMs);
            int delta = shouldHaveSent - revertRelSmooth.sent[i];
            if (delta != 0) {
                if (controls.getEncoder(i).isFunction)
                    sendRelativeCC(MIDI_CC, controls.getEncoder(i).fnId, delta, 7, 0);
                else {
                    uint8_t ch  = controls.getEncoder(i).channel;
                    ControlMidiType t = controls.getEncoder(i).type;
                    uint8_t num = controls.getEncoder(i).number;
                    sendRelativeCC(t, num, delta, ch);
                }
                revertRelSmooth.sent[i] = shouldHaveSent;
            }
        }
        if (done) revertRelSmooth.active = false;
    }
}

void initActionSettings() {
    unsigned long sec = screenSaverDelay / 1000UL;
    screensaverLevel = 0;
    for (int i = SS_STEPS - 1; i > 0; --i)
        if (sec >= ssStepsSec[i]) { screensaverLevel = i; break; }
    int savedLS = json.getDoc()["latchSmooth"]  | 0;
    int savedRS = json.getDoc()["revertSmooth"] | 0;
    latchSmoothLevel  = (uint8_t)constrain(savedLS, 0, 127);
    revertSmoothLevel = (uint8_t)constrain(savedRS, 0, 127);
    latchSmoothSync      = json.getDoc()["latchSmoothSync"]  | false;
    revertSmoothSync     = json.getDoc()["revertSmoothSync"] | false;
    latchSmoothSyncStep  = (uint8_t)constrain((int)(json.getDoc()["latchSmoothSyncStep"]  | 2), 0, SYNC_STEPS - 1);
    revertSmoothSyncStep = (uint8_t)constrain((int)(json.getDoc()["revertSmoothSyncStep"] | 2), 0, SYNC_STEPS - 1);
    uint8_t p = controls.getPreset();
    navPresetCursor = (p < 24) ? p : 0;
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
    for (int i = 0; i < 6; ++i) {
        strncpy(settingsSaved[i].paramName,  faders[i]->paramName,  20);
        strncpy(settingsSaved[i].title,      faders[i]->title,      20);
        strncpy(settingsSaved[i].buttonName, faders[i]->buttonName, 20);
        strncpy(settingsSaved[i].buttonText, faders[i]->buttonText, 20);
        settingsSaved[i].value             = faders[i]->value;
        settingsSaved[i].disabled          = faders[i]->disabled;
        settingsSaved[i].dimmed            = faders[i]->dimmed;
        settingsSaved[i].showingValue      = faders[i]->showingValue;
        settingsSaved[i].valueOnly         = faders[i]->valueOnly;
        settingsSaved[i].isQuantized       = faders[i]->isQuantized;
        settingsSaved[i].numQuantizedItems = faders[i]->numQuantizedItems;
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
    faders[0]->setQuantized(3);
    faders[0]->setParamName("Brightness");
    faders[0]->value = brightnessLevel * 63;
    faders[0]->setButtonName(layoutName);
    faders[0]->draw();
    faders[1]->disabled   = false;
    faders[1]->dimmed     = false;
    faders[1]->valueOnly  = false;
    faders[1]->showingValue = false;
    faders[1]->setQuantized(SS_STEPS);
    faders[1]->setParamName("Screensaver");
    faders[1]->value = screensaverLevel * 18;
    faders[1]->setButtonName(ssStepLabels[screensaverLevel]);
    faders[1]->draw();
    faders[2]->disabled   = false;
    faders[2]->dimmed     = false;
    faders[2]->valueOnly  = false;
    faders[2]->showingValue = false;
    faders[2]->setParamName("Latch Smooth");
    faders[2]->setQuantized(latchSmoothSync ? SYNC_STEPS : 0);
    if (latchSmoothSync) {
        faders[2]->value = latchSmoothSyncStep * 127 / (SYNC_STEPS - 1);
        strncpy(faders[2]->title, syncStepLabels[latchSmoothSyncStep], 20);
        strncpy(faders[2]->buttonName, "Sync", 20);
        strncpy(faders[2]->buttonText, "Sync", 20);
    } else {
        faders[2]->value = latchSmoothLevel;
        formatSmoothLabel(latchSmoothLevel, false, 0, faders[2]->title, 20);
        strncpy(faders[2]->buttonName, "Time", 20);
        strncpy(faders[2]->buttonText, "Time", 20);
    }
    faders[2]->buttonState = latchSmoothSync;
    faders[2]->draw();
    faders[3]->disabled   = false;
    faders[3]->dimmed     = false;
    faders[3]->valueOnly  = false;
    faders[3]->showingValue = false;
    faders[3]->setParamName("Revert Smooth");
    faders[3]->setQuantized(revertSmoothSync ? SYNC_STEPS : 0);
    if (revertSmoothSync) {
        faders[3]->value = revertSmoothSyncStep * 127 / (SYNC_STEPS - 1);
        strncpy(faders[3]->title, syncStepLabels[revertSmoothSyncStep], 20);
        strncpy(faders[3]->buttonName, "Sync", 20);
        strncpy(faders[3]->buttonText, "Sync", 20);
    } else {
        faders[3]->value = revertSmoothLevel;
        formatSmoothLabel(revertSmoothLevel, false, 0, faders[3]->title, 20);
        strncpy(faders[3]->buttonName, "Time", 20);
        strncpy(faders[3]->buttonText, "Time", 20);
    }
    faders[3]->buttonState = revertSmoothSync;
    faders[3]->draw();
    faders[4]->disabled    = true;
    faders[4]->dimmed      = false;
    faders[4]->valueOnly   = false;
    faders[4]->showingValue = false;
    faders[4]->isQuantized = false;
    faders[4]->buttonState = !ledsDimOff;
    strncpy(faders[4]->buttonText, "LEDs", 20);
    strncpy(faders[4]->buttonName, "LEDs", 20);
    faders[4]->draw();
    faders[5]->disabled    = true;
    faders[5]->dimmed      = false;
    faders[5]->valueOnly   = false;
    faders[5]->showingValue = false;
    faders[5]->isQuantized = false;
    faders[5]->buttonState = deviceLocked;
    strncpy(faders[5]->buttonText, "Dev.Lock", 20);
    strncpy(faders[5]->buttonName, "Dev.Lock", 20);
    faders[5]->draw();
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
        for (int i = 0; i < 6; ++i) {
            strncpy(faders[i]->paramName,  settingsSaved[i].paramName,  20);
            strncpy(faders[i]->title,      settingsSaved[i].title,      20);
            strncpy(faders[i]->buttonName, settingsSaved[i].buttonName, 20);
            strncpy(faders[i]->buttonText, settingsSaved[i].buttonText, 20);
            faders[i]->value             = settingsSaved[i].value;
            faders[i]->disabled          = settingsSaved[i].disabled;
            faders[i]->dimmed            = settingsSaved[i].dimmed;
            faders[i]->showingValue      = settingsSaved[i].showingValue;
            faders[i]->valueOnly         = settingsSaved[i].valueOnly;
            faders[i]->isQuantized       = settingsSaved[i].isQuantized;
            faders[i]->numQuantizedItems = settingsSaved[i].numQuantizedItems;
        }
        json.getDoc()["screensaver"] = (int)ssStepsSec[screensaverLevel];
        json.getDoc()["brightness"]  = (int)brightnessLevel;
        json.getDoc()["layout"]            = (int)faderLayout;
        json.getDoc()["latchSmooth"]         = (int)latchSmoothLevel;
        json.getDoc()["revertSmooth"]        = (int)revertSmoothLevel;
        json.getDoc()["latchSmoothSync"]     = (bool)latchSmoothSync;
        json.getDoc()["revertSmoothSync"]    = (bool)revertSmoothSync;
        json.getDoc()["latchSmoothSyncStep"] = (int)latchSmoothSyncStep;
        json.getDoc()["revertSmoothSyncStep"]= (int)revertSmoothSyncStep;
        json.getDoc()["ledsDimOff"]        = (bool)ledsDimOff;
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

    if (settingsModeActive) {
        shiftState = SHIFT_IDLE;
        return;
    }

    switch (shiftState) {
    case SHIFT_DOWN1:
        shiftPressed = false;
        setLed(1, false);
        sendMidiMessage(0, 110, 0, 7);
        shiftState = SHIFT_UP1;
        shiftStateTime = now;
        break;

    case SHIFT_HOLD:
        presetJustSelected = false;
        shiftState = SHIFT_IDLE;
        shiftPressed = false;
        setLed(1, false);
        sendMidiMessage(0, 110, 0, 7);
        showDisplay();
        for (int i = 0; i < 8; ++i)
            faders[i]->updateButtonName(faders[i]->buttonState);
        flushDisplays();
        sendPresetSysEx(controls.getPreset());
        break;

    case SHIFT_DOWN2:
        shiftPressed = false;
        if (now - shiftStateTime < SHIFT_DTHOLD_MS) {
            enterSettingsMode();
            drawSettingsPage();
        }
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

void checkPendingPresetSave()
{
    if (!pendingPresetSave && !pendingFullRedraw) return;
    unsigned long elapsed = millis() - lastPresetNavTime;
    if (pendingFullRedraw && elapsed >= 80) {
        pendingFullRedraw = false;
        updateFaderTitles();
        updateFaderValues();
        drawAllWidgets();
    }
    if (pendingPresetSave && elapsed >= 500) {
        pendingPresetSave = false;
        json.save();
    }
}

void checkShiftPending()
{
    if (pendingSmoothSave && millis() - pendingSmoothSaveTime >= 500) {
        pendingSmoothSave = false;
        json.save();
    }
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
            shiftState = SHIFT_IDLE;
            showDisplay();
            for (int i = 0; i < 8; ++i)
                faders[i]->updateButtonName(faders[i]->buttonState);
            flushDisplays();
        }
        break;
    case SHIFT_DOWN2:
        if (now - shiftStateTime >= SHIFT_DTHOLD_MS) {
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
    if (settingsModeActive) {
        if (idx == 0) {
            faderLayout = (faderLayout == LAYOUT_DYNAMIC) ? LAYOUT_COMPACT : LAYOUT_DYNAMIC;
            drawSettingsPage();
            { uint8_t pkt[5] = {0xF0, 0x6F, 0x0E, (uint8_t)faderLayout, 0xF7};
              serialEditorSend(pkt, 5); }
        } else if (idx == 2) {
            latchSmoothSync = !latchSmoothSync;
            faders[2]->setQuantized(latchSmoothSync ? SYNC_STEPS : 0);
            drawSettingsPage();
            json.getDoc()["latchSmoothSync"]     = (bool)latchSmoothSync;
            json.getDoc()["latchSmoothSyncStep"] = (int)latchSmoothSyncStep;
            settingsPendingSave = true;
            { uint8_t pkt[7] = {0xF0, 0x6F, 0x24, latchSmoothLevel, (uint8_t)(latchSmoothSync ? 1 : 0), latchSmoothSyncStep, 0xF7};
              serialEditorSend(pkt, 7); }
        } else if (idx == 3) {
            revertSmoothSync = !revertSmoothSync;
            faders[3]->setQuantized(revertSmoothSync ? SYNC_STEPS : 0);
            drawSettingsPage();
            json.getDoc()["revertSmoothSync"]     = (bool)revertSmoothSync;
            json.getDoc()["revertSmoothSyncStep"] = (int)revertSmoothSyncStep;
            settingsPendingSave = true;
            { uint8_t pkt[7] = {0xF0, 0x6F, 0x25, revertSmoothLevel, (uint8_t)(revertSmoothSync ? 1 : 0), revertSmoothSyncStep, 0xF7};
              serialEditorSend(pkt, 7); }
        } else if (idx == 4) {
            ledsDimOff = !ledsDimOff;
            applyLedDimOff();
            faders[4]->buttonState = !ledsDimOff;
            faders[4]->draw();
            flushDisplays();
            { uint8_t pkt[5] = {0xF0, 0x6F, 0x29, ledsDimOff ? (uint8_t)1 : (uint8_t)0, 0xF7};
              serialEditorSend(pkt, 5); }
        } else if (idx == 5) {
            deviceLocked = !deviceLocked;
            faders[5]->buttonState = deviceLocked;
            faders[5]->draw();
            flushDisplays();
            sendMidiMessage(MIDI_CC, 36, 127, 7, 0);
        }
        return;
    }
    if (shiftState == SHIFT_HOLD && !presetJustSelected)
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
        uint8_t oldPreset = controls.getPreset();
        int oldIdx = -1;
        if (oldPreset == 24) oldIdx = 6;
        else if (oldPreset == 25) oldIdx = 7;
        else { int rel = (int)oldPreset - (int)(presetTable * 6); if (rel >= 0 && rel < 6) oldIdx = rel; }
        if (oldIdx >= 0 && oldIdx != idx) {
            char ol1[20], ol2[20] = {0};
            if (oldIdx == 6)      { strcpy(ol1, "Global"); strcpy(ol2, "Mode"); }
            else if (oldIdx == 7) { strcpy(ol1, "Device"); strcpy(ol2, "Mode"); }
            else {
                uint8_t op = presetTable * 6 + oldIdx;
                snprintf(ol1, sizeof(ol1), "Preset %d", op + 1);
                const char* opn = controls.getPresetName(op);
                if (opn[0] != '\0') strncpy(ol2, opn, sizeof(ol2) - 1);
            }
            faders[oldIdx]->drawPresetButton(ol1, ol2, false);
        }
        faders[idx]->drawPresetButton(l1, l2, true);
        flushDisplays();
        if (newPreset < 24) navPresetCursor = newPreset;
        controls.setPreset(newPreset);
        json.save();
        updateFaderTitles();
        updateFaderValues();
        showDisplay();
        sendPresetSysEx(newPreset);
        presetJustSelected = true;
        return;
    }
    else
    {
        uint8_t channel = controls.getButtonShort(idx).channel;
        ControlMidiType type = controls.getButtonShort(idx).type;
        uint8_t number = controls.getButtonShort(idx).number;
        uint8_t _value;
        bool isFn = controls.getButtonShort(idx).isFunction;
        uint8_t pm = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).pressMode : 0;
        if (pm == 1) { // Toggle
            bool newState = !faders[idx]->buttonState;
            faders[idx]->buttonState = newState;
            faders[idx]->updateButtonName(newState);
            _value = newState ? controls.getButtonShort(idx).maxVal : controls.getButtonShort(idx).minVal;
        } else { // Momentary or Trigger: send max on press
            _value = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).maxVal : 127;
        }
        controls.getButtonShort(idx).value = _value;
        if (controls.getButtonShort(idx).isFunction) {
            uint8_t fnId = controls.getButtonShort(idx).fnId;
            if (fnId == 30 || fnId == 31) {
                uint8_t cur = controls.getPreset();
                uint8_t newPreset = (fnId == 31) ? min((int)cur + 1, 24) : max((int)cur - 1, 0);
                if (newPreset != cur) {
                    if (newPreset < 24) navPresetCursor = newPreset;
                    controls.setPreset(newPreset);
                    pendingPresetSave = true;
                    pendingFullRedraw = true;
                    lastPresetNavTime = millis();
                    if (newPreset == 24) {
                        strncpy(deviceLabel, "Track", sizeof(deviceLabel));
                        strncpy(bankLabel, "Global", sizeof(bankLabel));
                    } else {
                        snprintf(deviceLabel, sizeof(deviceLabel), "Preset %d", newPreset + 1);
                        const char* pName = controls.getPresetName(newPreset);
                        if (pName[0] != '\0')
                            strncpy(bankLabel, pName, sizeof(bankLabel));
                        else
                            snprintf(bankLabel, sizeof(bankLabel), "Preset %d", newPreset + 1);
                    }
                    deviceLabelDirty = true;
                    bankLabelDirty = true;
                    sendPresetSysEx(newPreset);
                }
            } else if (fnId >= 120 && fnId <= 143) {
                uint8_t targetPreset = fnId - 120;
                uint8_t cur = controls.getPreset();
                if (targetPreset != cur) {
                    navPresetCursor = targetPreset;
                    controls.setPreset(targetPreset);
                    pendingPresetSave = true;
                    pendingFullRedraw = true;
                    lastPresetNavTime = millis();
                    snprintf(deviceLabel, sizeof(deviceLabel), "Preset %d", targetPreset + 1);
                    const char* pName = controls.getPresetName(targetPreset);
                    if (pName[0] != '\0')
                        strncpy(bankLabel, pName, sizeof(bankLabel));
                    else
                        snprintf(bankLabel, sizeof(bankLabel), "Preset %d", targetPreset + 1);
                    deviceLabelDirty = true;
                    bankLabelDirty = true;
                    sendPresetSysEx(targetPreset);
                }
            } else {
                sendMidiMessage(MIDI_CC, fnId, _value, 7, 0);
            }
        } else {
            sendMidiMessage(type, number, _value, channel);
        }
        if (controls.getPreset() >= 24 || isFn) {
            faders[idx]->buttonHeld = true;
            faders[idx]->updateButtonName(true);
            flushDisplays();
        }
    }
}

void onButtonReleased(uint8_t idx)
{
    if (shiftState == SHIFT_HOLD) return;
    if (settingsModeActive) return;
    uint8_t channel = controls.getButtonShort(idx).channel;
    ControlMidiType type = controls.getButtonShort(idx).type;
    uint8_t number = controls.getButtonShort(idx).number;
    bool isFn = controls.getButtonShort(idx).isFunction;
    uint8_t pm = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).pressMode : 0;
    if (pm == 0 && type != MIDI_PC) { // Momentary only: send min on release
        uint8_t releaseVal = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).minVal : 0;
        if (controls.getButtonShort(idx).isFunction)
            sendMidiMessage(MIDI_CC, controls.getButtonShort(idx).fnId, releaseVal, 7, 0);
        else
            sendMidiMessage(type, number, releaseVal, channel);
        controls.getButtonShort(idx).value = releaseVal;
    }
    lastButtonReleaseTime[idx] = millis();
    if (controls.getPreset() >= 24 || isFn) {
        faders[idx]->buttonHeld = false;
        faders[idx]->updateButtonName(false);
        flushDisplays();
    }
}

void sendRelativeCC(uint8_t type, uint8_t number, int delta, uint8_t channel, uint8_t cable)
{
    const int MAX_STEP = 63;
    int remaining = delta;
    while (remaining != 0) {
        int step = constrain(remaining, -MAX_STEP, MAX_STEP);
        uint8_t relValue = (uint8_t)(step & 0x7F);
        sendMidiMessage(type, number, relValue, channel, cable);
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
    uint8_t pkt[5] = {0xF0, 0x6F, 0x1B, brightnessLevel, 0xF7};
    serialEditorSend(pkt, 5);
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
    uint16_t sec = (uint16_t)ssStepsSec[screensaverLevel];
    uint8_t pkt[6] = {0xF0, 0x6F, 0x10, (uint8_t)((sec >> 7) & 0x7F), (uint8_t)(sec & 0x7F), 0xF7};
    serialEditorSend(pkt, 6);
}

static void handleSettingsLatchSmooth(int delta) {
    if (latchSmoothSync) {
        int s = (int)latchSmoothSyncStep + (delta > 0 ? 1 : -1);
        latchSmoothSyncStep = (uint8_t)constrain(s, 0, SYNC_STEPS - 1);
        faders[2]->value = latchSmoothSyncStep * 127 / (SYNC_STEPS - 1);
        strncpy(faders[2]->title, syncStepLabels[latchSmoothSyncStep], 20);
    } else {
        latchSmoothLevel = (uint8_t)constrain((int)latchSmoothLevel + delta, 0, 127);
        faders[2]->value = latchSmoothLevel;
        formatSmoothLabel(latchSmoothLevel, false, 0, faders[2]->title, 20);
    }
    faders[2]->showingValue = true;
    controls.getEncoder(2).lastActivity = millis();
    faders[2]->draw();
    flushDisplays();
    json.getDoc()["latchSmooth"]         = (int)latchSmoothLevel;
    json.getDoc()["latchSmoothSync"]     = (bool)latchSmoothSync;
    json.getDoc()["latchSmoothSyncStep"] = (int)latchSmoothSyncStep;
    pendingSmoothSave = true;
    pendingSmoothSaveTime = millis();
    uint8_t pkt[7] = {0xF0, 0x6F, 0x24, latchSmoothLevel, (uint8_t)(latchSmoothSync ? 1 : 0), latchSmoothSyncStep, 0xF7};
    serialEditorSend(pkt, 7);
}

static void handleSettingsRevertSmooth(int delta) {
    if (revertSmoothSync) {
        int s = (int)revertSmoothSyncStep + (delta > 0 ? 1 : -1);
        revertSmoothSyncStep = (uint8_t)constrain(s, 0, SYNC_STEPS - 1);
        faders[3]->value = revertSmoothSyncStep * 127 / (SYNC_STEPS - 1);
        strncpy(faders[3]->title, syncStepLabels[revertSmoothSyncStep], 20);
    } else {
        revertSmoothLevel = (uint8_t)constrain((int)revertSmoothLevel + delta, 0, 127);
        faders[3]->value = revertSmoothLevel;
        formatSmoothLabel(revertSmoothLevel, false, 0, faders[3]->title, 20);
    }
    faders[3]->showingValue = true;
    controls.getEncoder(3).lastActivity = millis();
    faders[3]->draw();
    flushDisplays();
    json.getDoc()["revertSmooth"]         = (int)revertSmoothLevel;
    json.getDoc()["revertSmoothSync"]     = (bool)revertSmoothSync;
    json.getDoc()["revertSmoothSyncStep"] = (int)revertSmoothSyncStep;
    pendingSmoothSave = true;
    pendingSmoothSaveTime = millis();
    uint8_t pkt[7] = {0xF0, 0x6F, 0x25, revertSmoothLevel, (uint8_t)(revertSmoothSync ? 1 : 0), revertSmoothSyncStep, 0xF7};
    serialEditorSend(pkt, 7);
}

static void navigatePresetByEncoder(uint8_t idx, int delta) {
    int newCursor = (int)navPresetCursor + (delta > 0 ? 1 : -1);
    newCursor = constrain(newCursor, 0, 23);
    if ((uint8_t)newCursor == navPresetCursor) return;
    navPresetCursor = (uint8_t)newCursor;
    controls.setPreset(navPresetCursor);
    pendingPresetSave = true;
    pendingFullRedraw = true;
    lastPresetNavTime = millis();
    snprintf(deviceLabel, sizeof(deviceLabel), "Preset %d", navPresetCursor + 1);
    const char* pName = controls.getPresetName(navPresetCursor);
    if (pName[0] != '\0')
        strncpy(bankLabel, pName, sizeof(bankLabel));
    else
        snprintf(bankLabel, sizeof(bankLabel), "Preset %d", navPresetCursor + 1);
    deviceLabelDirty = true;
    bankLabelDirty = true;
    sendPresetSysEx(navPresetCursor);
    faders[idx]->setValue((uint8_t)constrain((int)navPresetCursor * 127 / 23, 0, 127));
}

void onRelativeEncoderChange(uint8_t idx, int delta)
{
    if (settingsModeActive) {
        if      (idx == 0) handleSettingsBrightness(delta);
        else if (idx == 1) handleSettingsScreensaver(delta);
        else if (idx == 2) handleSettingsLatchSmooth(delta);
        else if (idx == 3) handleSettingsRevertSmooth(delta);
        return;
    }
    if (shiftState == SHIFT_HOLD) {
        if (delta > 0 && presetTable < 3) presetTable++;
        else if (delta < 0 && presetTable > 0) presetTable--;
        drawPresetPage();
        return;
    }

    if (controls.getEncoder(idx).isFunction && controls.getEncoder(idx).fnId == 58) {
        navigatePresetByEncoder(idx, delta);
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

    bool encIsFunction = controls.getEncoder(idx).isFunction;
    if (revertMode)
    {
        revertAbsoluteChanged[idx] = true;
        if (encIsFunction)
            sendRelativeCC(MIDI_CC, controls.getEncoder(idx).fnId, delta, 7, 0);
        else
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
        if (encIsFunction)
            sendRelativeCC(MIDI_CC, controls.getEncoder(idx).fnId, delta, 7, 0);
        else
            sendRelativeCC(type, number, delta, channel);
    }

    if (latchPressed || latchPendingActivation || controls.getPreset() < 24)
    {
        updateFader(idx, (uint8_t)estimated_display);
        char buffer[16];
        sprintf(buffer, "%d", estimated_display);
        if (!controls.getEncoder(idx).hasWatcher && !encIsFunction)
            faders[idx]->updateTitle(buffer);
    }
}

void onAbsoluteEncoderChange(uint8_t idx, int delta)
{
    if (settingsModeActive) {
        if      (idx == 0) handleSettingsBrightness(delta);
        else if (idx == 1) handleSettingsScreensaver(delta);
        else if (idx == 2) handleSettingsLatchSmooth(delta);
        else if (idx == 3) handleSettingsRevertSmooth(delta);
        return;
    }
    if (shiftState == SHIFT_HOLD) {
        if (delta > 0 && presetTable < 3) presetTable++;
        else if (delta < 0 && presetTable > 0) presetTable--;
        drawPresetPage();
        return;
    }

    if (controls.getEncoder(idx).isFunction && controls.getEncoder(idx).fnId == 58) {
        navigatePresetByEncoder(idx, delta);
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
        faders[i]->buttonHeld = false;
        faders[i]->valueOnly = (preset == 24 && (i == 2 || i == 3 || i == 6));
        bool isPresetSelector = enc.isFunction && enc.fnId == 58;
        faders[i]->isQuantized = isPresetSelector;
        faders[i]->numQuantizedItems = isPresetSelector ? 24 : 0;
        if (isPresetSelector)
            controls.getEncoder(i).value = (uint8_t)constrain((int)navPresetCursor * 127 / 23, 0, 127);
        if (faders[i]->valueOnly) {
            faders[i]->showingValue = true;
            faders[i]->updateTitle("---");
        } else {
            faders[i]->showingValue = false;
        }
        if (isPresetSelector) {
            faders[i]->setParamName("Sel. Preset");
        } else if (enc.controlName[0] != '\0') {
            faders[i]->setParamName(enc.controlName);
        } else if (preset == 25 || enc.isFunction) {
            faders[i]->setParamName(enc.isFunction ? "" : "---");
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
                "Track -", "Track +", "Device On", "Capture",
                "Device -", "Device +", "Bank -", "Bank +"
            };
            snprintf(buf, sizeof(buf), buttonNames[i]);
        }
        else if (controls.getPreset() == 24) {
            MidiControl& btn = controls.getButtonShort(i);
            snprintf(buf, sizeof(buf), "%s", btn.controlName[0] != '\0' ? btn.controlName : "");
        }
        else {
        MidiControl& btn = controls.getButtonShort(i);
        if (btn.controlName[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s", btn.controlName);
        } else if (btn.isFunction) {
            snprintf(buf, sizeof(buf), "---");
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
        strncpy(trackLabel, "Track", sizeof(trackLabel));
        strncpy(deviceLabel, "Device", sizeof(deviceLabel));
        strncpy(bankLabel, "Bank", sizeof(bankLabel));
        strncpy(presetLabel, "Device", sizeof(presetLabel) - 1);
        presetLabel[sizeof(presetLabel) - 1] = '\0';
        trackLabelDirty = true;
        deviceLabelDirty = true;
        bankLabelDirty = true;
        presetLabelDirty = true;
    } else if (preset == 24) {
        strncpy(deviceLabel, "Track", sizeof(deviceLabel));
        strncpy(bankLabel, "Global", sizeof(bankLabel));
        strncpy(presetLabel, "Global", sizeof(presetLabel) - 1);
        presetLabel[sizeof(presetLabel) - 1] = '\0';
        deviceLabelDirty = true;
        bankLabelDirty = true;
        presetLabelDirty = true;
    } else {
        strncpy(trackLabel, "Track", sizeof(trackLabel));
        trackLabelDirty = true;
        strncpy(deviceLabel, "Device", sizeof(deviceLabel));
        deviceLabelDirty = true;
        strncpy(bankLabel, "Bank", sizeof(bankLabel));
        bankLabelDirty = true;
        const char* pName = controls.getPresetName(preset);
        if (pName && pName[0] != '\0') {
            strncpy(presetLabel, pName, sizeof(presetLabel) - 1);
        } else {
            snprintf(presetLabel, sizeof(presetLabel), "Preset %d", preset + 1);
        }
        presetLabel[sizeof(presetLabel) - 1] = '\0';
        presetLabelDirty = true;
    }
    for (int i = 0; i < 4; i++)
        currentLabelConfig[i] = controls.getLabelConfig(preset, i);
    // Restore device lock LED for buttons with fnId 36
    for (int i = 0; i < 8; ++i) {
        if (controls.getButtonShort(i).isFunction && controls.getButtonShort(i).fnId == 36)
            faders[i]->buttonState = deviceLocked;
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
    serialEditorSend(packet, 5);
}

void releaseLatchAndSend()
{
    int smoothMs = latchSmoothSync ? syncSmoothToMs(latchSmoothSyncStep) : smoothLevelToMs(latchSmoothLevel);

    for (int i = 0; i < 8; ++i)
    {
        if (latchAbsoluteChanged[i])
        {
            ControlMidiType type = controls.getEncoder(i).type;
            if (smoothMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB) {
                startAbsoluteSmooth(i, latchOriginalValue[i], latchAbsoluteValue[i], smoothMs);
            } else {
                uint8_t channel = controls.getEncoder(i).channel;
                uint8_t number  = controls.getEncoder(i).number;
                sendMidiMessage(type, number, latchAbsoluteValue[i], channel);
            }
            latchAbsoluteChanged[i] = false;
        }
    }

    if (smoothMs > 0) {
        int maxAcc = 0;
        for (int i = 0; i < 8; ++i)
            if (abs(latchRelAccum[i]) > maxAcc) maxAcc = abs(latchRelAccum[i]);
        if (maxAcc > 0) {
            relSmooth.active    = true;
            relSmooth.startTime = millis();
            relSmooth.totalMs   = (unsigned long)smoothMs;
            for (int i = 0; i < 8; ++i) { relSmooth.target[i] = latchRelAccum[i]; relSmooth.sent[i] = 0; }
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
    int smoothMs = revertSmoothSync ? syncSmoothToMs(revertSmoothSyncStep) : smoothLevelToMs(revertSmoothLevel);
    revertRelSmooth = {false, {0}, {0}, 0, 0};
    for (int i = 0; i < 8; ++i)
    {
        if (!revertAbsoluteChanged[i])
            continue;
        ControlMidiType type = controls.getEncoder(i).type;
        uint8_t channel = controls.getEncoder(i).channel;
        uint8_t number  = controls.getEncoder(i).number;
        bool encIsRelative = controls.getPreset() > 23 || controls.getEncoder(i).isFunction;

        if (encIsRelative)
        {
            int delta = (int)revertAbsoluteOriginal[i] - (int)controls.getEncoder(i).value;
            controls.getEncoder(i).value = revertAbsoluteOriginal[i];
            updateFader(i, revertAbsoluteOriginal[i]);
            if (smoothMs > 0 && delta != 0) {
                revertRelSmooth.target[i] = delta;
            } else if (delta != 0) {
                if (controls.getEncoder(i).isFunction)
                    sendRelativeCC(MIDI_CC, controls.getEncoder(i).fnId, delta, 7, 0);
                else
                    sendRelativeCC(type, number, delta, channel);
            }
        }
        else if (smoothMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB)
        {
            startAbsoluteSmooth(i, controls.getEncoder(i).value, revertAbsoluteOriginal[i], smoothMs);
            // value/fader updated incrementally by checkSmoothProgress()
        }
        else
        {
            sendMidiMessage(type, number, revertAbsoluteOriginal[i], channel);
            controls.getEncoder(i).value = revertAbsoluteOriginal[i];
            updateFader(i, revertAbsoluteOriginal[i]);
        }
        revertAbsoluteChanged[i] = false;
    }
    bool hasRelTargets = false;
    for (int i = 0; i < 8; ++i) if (revertRelSmooth.target[i] != 0) { hasRelTargets = true; break; }
    if (hasRelTargets) {
        revertRelSmooth.active = true;
        revertRelSmooth.startTime = millis();
        revertRelSmooth.totalMs = (unsigned long)smoothMs;
    }
}

void setRevertModeLed(bool on)
{
    setLedBlink(0, on);
}

void setDeviceLockState(bool locked)
{
    deviceLocked = locked;
    if (settingsModeActive) {
        faders[5]->buttonState = locked;
        drawSettingsPage();
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (controls.getButtonShort(i).isFunction && controls.getButtonShort(i).fnId == 36) {
            faders[i]->buttonState = locked;
            faders[i]->updateButtonName(locked);
            faders[i]->_dirty = true;
        }
    }
    flushDisplays();
}

void onSmoothStateUpdatedFromEditor()
{
    if (settingsModeActive) drawSettingsPage();
}
