#include <Arduino.h>
#include <PicoGFX_SSD1322.h>
#include <SPI.h>
#include <hardware/watchdog.h>

#include "midi/midi.h"
#include "usb/serial_editor.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
USING_NAMESPACE_MIDI
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// SPI: SCK=GPIO6, MOSI=GPIO7 (set in setup())
PicoGFX_SSD1322 display1(256, 64, &SPI, 9, 8, 10);   // DC=9, RST=8, CS=10
PicoGFX_SSD1322 display2(256, 64, &SPI, 12, 11, 13); // DC=12, RST=11, CS=13
#include "core/jsonManager.h"
#include "core/controls.h"
#include "input/encoders.h"
#include "input/buttons.h"
#include "view/leds.h"
#include "view/display.h"
#include "core/actions.h"
#include "sequencer/sequencer.h"

unsigned long screenSaverDelay = 300000; // 5 minutes par défaut

void setup() {
  Serial.begin(115200);
  delay(100);
  setupJsonManager();
  setupMIDI();
  SPI.setSCK(6);  // Set SCK to GPIO 6
  SPI.setTX(7);   // Set MOSI to GPIO 7
  SPI.begin();
  delay(100);
  setupDisplay();
  controls.setDefaults();
  int savedLayout = json.getLayout();
  if (savedLayout >= 0 && savedLayout <= 2)
    faderLayout = static_cast<FaderLayout>(savedLayout);
  int savedSS = json.getDoc()["screensaver"] | 300;
  screenSaverDelay = (unsigned long)savedSS * 1000UL;
  int savedBrightness = json.getDoc()["brightness"] | 1;
  setBrightness(savedBrightness);
  updateFaderTitles();
  updateFaderValues();
  encoders.setup();
  setupButtons();
  setupLeds();
  sequencer.setup();
  watchdog_enable(8000, true);
  Serial.println("=== STARTUP COMPLETE ===");
  delay(100);

}

unsigned long lastDisplay = 0;
const unsigned long displayInterval = 100;  // 20 FPS
unsigned long lastInactivityCheck = 0;
const unsigned long inactivityCheckInterval = 250;
bool bootPresetSent = false;

void loop() {
    watchdog_update();
    midiRead();

    if (!bootPresetSent && millis() > 2000) {
        bootPresetSent = true;
        sendPresetSysEx(controls.getPreset());
    }
    serialEditorRead();
    encoders.read();
    readButtons();
    checkLatchPending();
    checkNamingPending();
    updateLeds();
    updateDisplay();

    unsigned long currentTime = millis();
    if (currentTime - lastInactivityCheck >= inactivityCheckInterval) {
        controls.checkInactiveEncoders();
        lastInactivityCheck = currentTime;
    }

    // Screen saver
    if (screenSaverDelay > 0 && !screenSaverActive && currentTime - lastInputTime > screenSaverDelay) {
        screenSaverActive = true;
    }
    if (screenSaverActive) {
        runScreenSaver();
    }
}
