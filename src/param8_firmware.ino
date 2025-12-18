#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
// #define PIN_SPI0_MOSI  (7u)
// #define PIN_SPI0_SCK   (6u)
//  U8G2_SSD1322_NHD_256X64_1_4W_SW_SPI u8g2(U8G2_MIRROR_VERTICAL, /* clock=*/ 6, /* data=*/ 7, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
//  U8G2_SSD1322_NHD_256X64_1_4W_SW_SPI u8g2_2(U8G2_MIRROR_VERTICAL, /* clock=*/ 6, /* data=*/ 7, /* cs=*/ 13, /* dc=*/ 12, /* reset=*/ 11);
// U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
// U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2_2(U8G2_R0, /* cs=*/ 13, /* dc=*/ 12, /* reset=*/ 11);

#include "midi/midi.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
USING_NAMESPACE_MIDI
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);


U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/10, /* dc=*/9, /* reset=*/8);
U8G2_SSD1322_ZJY_256X64_F_4W_HW_SPI u8g2_2(U8G2_R0, /* cs=*/13, /* dc=*/12, /* reset=*/11);
#include "core/jsonManager.h"
#include "core/controls.h"
#include "input/encoders.h"
#include "input/buttons.h"
#include "view/leds.h"
#include "view/display.h"
#include "core/actions.h"

#define SCREEN_SAVER_DELAY 60000 // 1 minute d'inactivité

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
  updateFaderTitles();
  sendPresetSysEx(controls.getPreset());
  encoders.setup();
  setupButtons();
//   setupLeds();
  Serial.println("=== STARTUP COMPLETE ===");
  delay(100);     // Stabilisation finale

}

unsigned long lastDisplay = 0;
const unsigned long displayInterval = 100;  // 20 FPS
unsigned long lastInactivityCheck = 0;
const unsigned long inactivityCheckInterval = 250;  // Vérifier toutes les 250ms

void loop() {
    midiRead();
    encoders.read();
    readButtons();
    updateDisplay();

    unsigned long currentTime = millis();
    if (currentTime - lastInactivityCheck >= inactivityCheckInterval) {
        controls.checkInactiveEncoders();
        lastInactivityCheck = currentTime;
    }

    // Screen saver
    if (!screenSaverActive && currentTime - lastInputTime > SCREEN_SAVER_DELAY) {
        screenSaverActive = true;
    }
    if (screenSaverActive) {
        // runScreenSaver();
    }
}
