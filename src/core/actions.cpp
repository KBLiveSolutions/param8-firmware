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
static FaderSaveState settingsSaved[4];

static const unsigned long ssStepsSec[] = {0, 60, 120, 300, 600, 1800, 3600, 7200};
static const char* const   ssStepLabels[] = {"Off", "1 min", "2 min", "5 min", "10 min", "30 min", "1 h", "2 h"};
#define SS_STEPS 8
static uint8_t screensaverLevel = 3;

static int slewLevelToMs(uint8_t level) {
    if (level == 0) return 0;
    return (int)roundf(10.0f * powf(300.0f, (level - 1) / 126.0f));
}
static void formatSlewLabel(uint8_t level, char* buf, size_t len) {
    if (level == 0) { strncpy(buf, "Off", len); return; }
    int ms = slewLevelToMs(level);
    if (ms < 1000) snprintf(buf, len, "%dms", ms);
    else           snprintf(buf, len, "%.1fs", ms / 1000.0f);
}
uint8_t latchSlewLevel  = 0;
uint8_t revertSlewLevel = 0;
static bool settingsPendingSave = false;

uint8_t revertAbsoluteOriginal[8] = {0};
bool revertAbsoluteChanged[8] = {false};

struct SlewOp {
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
static SlewOp slewOps[8];

struct RelSlewOp {
    bool active = false;
    int  target[8] = {0};
    int  sent[8]   = {0};
    unsigned long startTime = 0;
    unsigned long totalMs   = 0;
};
static RelSlewOp relSlew;

static void startAbsoluteSlew(uint8_t idx, int fromVal, int toVal, int slewMs) {
    uint8_t channel = controls.getEncoder(idx).channel;
    ControlMidiType type = controls.getEncoder(idx).type;
    uint8_t number  = controls.getEncoder(idx).number;
    int range = abs(toVal - fromVal);
    unsigned long totalMs = range > 0 ? (unsigned long)slewMs * range / 127 : 0;
    if (totalMs == 0) { sendMidiMessage(type, number, (uint8_t)toVal, channel); return; }
    slewOps[idx].active    = true;
    slewOps[idx].fromVal   = fromVal;
    slewOps[idx].toVal     = toVal;
    slewOps[idx].lastSent  = fromVal;
    slewOps[idx].startTime = millis();
    slewOps[idx].totalMs   = totalMs;
    slewOps[idx].type      = type;
    slewOps[idx].number    = number;
    slewOps[idx].channel   = channel;
}

// kept only to avoid touching the old dead-code path below:
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

bool isSelectingPreset() {
    return shiftState == SHIFT_HOLD;
}

void sendRelativeCC(uint8_t type, uint8_t number, int delta, uint8_t channel, uint8_t cable = 255);

// Returns display name for a given fnId (used in fader title display).
static const char* fnIdName(uint8_t fnId) {
    // Device Params 1-32 (fn_ids 80-111)
    if (fnId >= 80 && fnId <= 111) {
        static char buf[10];
        snprintf(buf, sizeof(buf), "Param %d", fnId - 79);
        return buf;
    }
    switch (fnId) {
        // Global buttons 0x01-0x09
        case  1: return "Play";
        case  2: return "Stop";
        case  3: return "Undo";
        case  4: return "Redo";
        case  5: return "Tempo +1";
        case  6: return "Tempo -1";
        case  7: return "Capture";
        case  8: return "Re-en. Auto";
        case  9: return "Metronome";
        // Arrangement buttons 0x0A-0x12
        case 10: return "Arr. Loop";
        case 11: return "Continue";
        case 12: return "Restart";
        case 13: return "Bk to Arr.";
        case 14: return "Arr. Rec.";
        case 15: return "Marker -";
        case 16: return "Marker +";
        case 17: return "Skip Fwd";
        case 18: return "Skip Bwd";
        // Session buttons 0x13-0x17
        case 19: return "Launch Sc.";
        case 20: return "Scene +";
        case 21: return "Scene -";
        case 22: return "Sess. Rec.";
        case 23: return "Stop Clips";
        // Navigation buttons 0x18-0x1F
        case 24: return "Track -";
        case 25: return "Track +";
        case 26: return "Device -";
        case 27: return "Device +";
        case 28: return "Bank -";
        case 29: return "Bank +";
        case 30: return "Preset -";
        case 31: return "Preset +";
        // Track/Device buttons 0x20-0x28
        case 32: return "Mute";
        case 33: return "Solo";
        case 34: return "Arm";
        case 35: return "Ret. Deflt.";
        case 36: return "Prev Var.";
        case 37: return "Next Var.";
        case 38: return "Laun. Var.";
        case 39: return "Store Var.";
        case 40: return "Randomize";
        // Mixer/Global encoders 0x30-0x3E
        case 48: return "Master Vol.";
        case 49: return "Cue Vol.";
        case 50: return "BPM";
        case 51: return "Scrl Scenes";
        case 52: return "Track Vol.";
        case 53: return "Track Pan";
        case 54: return "Song Pos.";
        case 55: return "Sel. Param.";
        case 56: return "Groove";
        case 57: return "Track Sel.";
        case 58: return "Sel. Preset";
        case 59: return "Loop Pos.";
        case 60: return "Loop Len.";
        case 61: return "H. Zoom";
        case 62: return "Jump Mkr.";
        // Sends A-L 0x40-0x4B
        case 64: return "Send A";
        case 65: return "Send B";
        case 66: return "Send C";
        case 67: return "Send D";
        case 68: return "Send E";
        case 69: return "Send F";
        case 70: return "Send G";
        case 71: return "Send H";
        case 72: return "Send I";
        case 73: return "Send J";
        case 74: return "Send K";
        case 75: return "Send L";
        default: return nullptr;
    }
}

// Maps isFunction fnIds to their channel-7 CC numbers.
// Encoders 48-55 → CC 40-47 (offset -8). Buttons mapped explicitly.
static uint8_t fnIdToGlobalCC(uint8_t fnId) {
    if (fnId >= 48 && fnId <= 55) return fnId - 8;  // Master Vol..Sel. Param → CC 40-47
    switch (fnId) {
        case  1: return 52;  // Play
        case  9: return 50;  // Metronome
        case 10: return 56;  // Arr. Loop
        case 19: return 53;  // Launch Scene
        case 32: return 54;  // Mute
        case 33: return 55;  // Solo
        case 34: return 51;  // Arm
        case 35: return 57;  // Return to Default
        default: return fnId;
    }
}

void checkSlewProgress() {
    unsigned long now = millis();

    for (int i = 0; i < 8; ++i) {
        if (!slewOps[i].active) continue;
        unsigned long elapsed = now - slewOps[i].startTime;
        bool done = elapsed >= slewOps[i].totalMs;
        int newVal = done
            ? slewOps[i].toVal
            : slewOps[i].fromVal + (int)((long)(slewOps[i].toVal - slewOps[i].fromVal) * (long)elapsed / (long)slewOps[i].totalMs);
        newVal = constrain(newVal, 0, 127);
        if (newVal != slewOps[i].lastSent) {
            sendMidiMessage(slewOps[i].type, slewOps[i].number, (uint8_t)newVal, slewOps[i].channel);
            controls.getEncoder(i).value = (uint8_t)newVal;
            updateFader(i, newVal);
            slewOps[i].lastSent = newVal;
        }
        if (done) slewOps[i].active = false;
    }

    if (relSlew.active) {
        unsigned long elapsed = now - relSlew.startTime;
        bool done = elapsed >= relSlew.totalMs;
        for (int i = 0; i < 8; ++i) {
            if (relSlew.target[i] == 0) continue;
            int shouldHaveSent = done
                ? relSlew.target[i]
                : (int)((long)relSlew.target[i] * (long)elapsed / (long)relSlew.totalMs);
            int delta = shouldHaveSent - relSlew.sent[i];
            if (delta != 0) {
                uint8_t ch  = controls.getEncoder(i).channel;
                ControlMidiType t = controls.getEncoder(i).type;
                uint8_t num = controls.getEncoder(i).number;
                sendRelativeCC(t, num, delta, ch);
                relSlew.sent[i] = shouldHaveSent;
            }
        }
        if (done) relSlew.active = false;
    }
}

void initActionSettings() {
    unsigned long sec = screenSaverDelay / 1000UL;
    screensaverLevel = 0;
    for (int i = SS_STEPS - 1; i > 0; --i)
        if (sec >= ssStepsSec[i]) { screensaverLevel = i; break; }
    int savedLS = json.getDoc()["latchSlew"]  | 0;
    int savedRS = json.getDoc()["revertSlew"] | 0;
    latchSlewLevel  = (uint8_t)constrain(savedLS, 0, 127);
    revertSlewLevel = (uint8_t)constrain(savedRS, 0, 127);
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
    faders[2]->setParamName("Latch Slew");
    faders[2]->value = latchSlewLevel;
    formatSlewLabel(latchSlewLevel, faders[2]->title, 20);
    faders[2]->showingValue = true;
    strncpy(faders[2]->buttonName, "Time", 20);
    strncpy(faders[2]->buttonText, "Time", 20);
    faders[2]->draw();
    faders[3]->disabled   = false;
    faders[3]->dimmed     = false;
    faders[3]->valueOnly  = false;
    faders[3]->setParamName("Revert Slew");
    faders[3]->value = revertSlewLevel;
    formatSlewLabel(revertSlewLevel, faders[3]->title, 20);
    faders[3]->showingValue = true;
    strncpy(faders[3]->buttonName, "Time", 20);
    strncpy(faders[3]->buttonText, "Time", 20);
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
        controls.setPreset(newPreset);
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
        bool isToggle = (controls.getPreset() < 24) && !isFn && controls.getButtonShort(idx).toggleMode;
        if (isToggle) {
            bool newState = !faders[idx]->buttonState;
            faders[idx]->buttonState = newState;
            faders[idx]->updateButtonName(newState);
            _value = newState ? controls.getButtonShort(idx).maxVal : controls.getButtonShort(idx).minVal;
        } else {
            _value = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).maxVal : 127;
        }
        controls.getButtonShort(idx).value = _value;
        if (controls.getButtonShort(idx).isFunction)
            sendMidiMessage(MIDI_CC, fnIdToGlobalCC(controls.getButtonShort(idx).fnId), _value, 7, 0);
        else
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
    if (shiftState == SHIFT_HOLD) return;
    uint8_t channel = controls.getButtonShort(idx).channel;
    ControlMidiType type = controls.getButtonShort(idx).type;
    uint8_t number = controls.getButtonShort(idx).number;
    bool isFn = controls.getButtonShort(idx).isFunction;
    bool isToggle = (controls.getPreset() < 24) && !isFn && controls.getButtonShort(idx).toggleMode;
    if (!isToggle && type != MIDI_PC) {
        uint8_t releaseVal = (controls.getPreset() < 24 && !isFn) ? controls.getButtonShort(idx).minVal : 0;
        if (controls.getButtonShort(idx).isFunction)
            sendMidiMessage(MIDI_CC, fnIdToGlobalCC(controls.getButtonShort(idx).fnId), releaseVal, 7, 0);
        else
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
    latchSlewLevel = (uint8_t)constrain((int)latchSlewLevel + delta, 0, 127);
    faders[2]->value = latchSlewLevel;
    formatSlewLabel(latchSlewLevel, faders[2]->title, 20);
    faders[2]->showingValue = true;
    faders[2]->draw();
    flushDisplays();
}

static void handleSettingsRevertSlew(int delta) {
    revertSlewLevel = (uint8_t)constrain((int)revertSlewLevel + delta, 0, 127);
    faders[3]->value = revertSlewLevel;
    formatSlewLabel(revertSlewLevel, faders[3]->title, 20);
    faders[3]->showingValue = true;
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

    bool encIsFunction = controls.getEncoder(idx).isFunction;
    if (revertMode)
    {
        revertAbsoluteChanged[idx] = true;
        if (encIsFunction)
            sendRelativeCC(MIDI_CC, fnIdToGlobalCC(controls.getEncoder(idx).fnId), delta, 7, 0);
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
            sendRelativeCC(MIDI_CC, fnIdToGlobalCC(controls.getEncoder(idx).fnId), delta, 7, 0);
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
        } else if (enc.isFunction) {
            const char* name = fnIdName(enc.fnId);
            faders[i]->setParamName(name ? name : "Function");
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
        } else if (btn.isFunction) {
            const char* name = fnIdName(btn.fnId);
            snprintf(buf, sizeof(buf), "%s", name ? name : "Function");
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
    int slewMs = slewLevelToMs(latchSlewLevel);

    for (int i = 0; i < 8; ++i)
    {
        if (latchAbsoluteChanged[i])
        {
            ControlMidiType type = controls.getEncoder(i).type;
            if (slewMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB) {
                startAbsoluteSlew(i, latchOriginalValue[i], latchAbsoluteValue[i], slewMs);
            } else {
                uint8_t channel = controls.getEncoder(i).channel;
                uint8_t number  = controls.getEncoder(i).number;
                sendMidiMessage(type, number, latchAbsoluteValue[i], channel);
            }
            latchAbsoluteChanged[i] = false;
        }
    }

    if (slewMs > 0) {
        int maxAcc = 0;
        for (int i = 0; i < 8; ++i)
            if (abs(latchRelAccum[i]) > maxAcc) maxAcc = abs(latchRelAccum[i]);
        if (maxAcc > 0) {
            unsigned long totalMs = (unsigned long)slewMs * maxAcc / 127;
            relSlew.active    = true;
            relSlew.startTime = millis();
            relSlew.totalMs   = totalMs;
            for (int i = 0; i < 8; ++i) { relSlew.target[i] = latchRelAccum[i]; relSlew.sent[i] = 0; }
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
    int slewMs = slewLevelToMs(revertSlewLevel);
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
            if (delta != 0) {
                if (controls.getEncoder(i).isFunction)
                    sendRelativeCC(MIDI_CC, fnIdToGlobalCC(controls.getEncoder(i).fnId), delta, 7, 0);
                else
                    sendRelativeCC(type, number, delta, channel);
            }
            controls.getEncoder(i).value = revertAbsoluteOriginal[i];
            updateFader(i, revertAbsoluteOriginal[i]);
        }
        else if (slewMs > 0 && !controls.getEncoder(i).hiRes && type != MIDI_PB)
        {
            startAbsoluteSlew(i, controls.getEncoder(i).value, revertAbsoluteOriginal[i], slewMs);
            // value/fader updated incrementally by checkSlewProgress()
        }
        else
        {
            sendMidiMessage(type, number, revertAbsoluteOriginal[i], channel);
            controls.getEncoder(i).value = revertAbsoluteOriginal[i];
            updateFader(i, revertAbsoluteOriginal[i]);
        }
        revertAbsoluteChanged[i] = false;
    }
}

void setRevertModeLed(bool on)
{
    setLedBlink(0, on);
}
