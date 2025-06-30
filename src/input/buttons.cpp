#include "buttons.h"
#include "../core/actions.h"

int b_pins[2] = {2, 1}; // GPIO pins for buttons
Adafruit_PCF8574 pcf;

const uint16_t longPressDuration = 1000; // ms, à ajuster
ButtonState buttons[NB_BUTTONS];

void updateButton(uint8_t idx, bool reading) {
  buttons[idx].lastState = buttons[idx].currentState;
  buttons[idx].currentState = reading;
  unsigned long now = millis();

  if (!buttons[idx].currentState) { // Bouton appuyé (active bas)
    if (buttons[idx].pressedTime == 0) {
      // Vient d'être pressé
      buttons[idx].pressedTime = now;
      buttons[idx].longPressEventFired = false;
      buttons[idx].longPressDetected = false;
      buttons[idx].shortPressEventPending = false;
      buttons[idx].longPressHasBeenFired = false;
    } else if (!buttons[idx].longPressEventFired &&
               (now - buttons[idx].pressedTime) >= longPressDuration &&
               !buttons[idx].longPressHasBeenFired) {
      // Long press à déclencher une seule fois
      buttons[idx].longPressEventFired = true;
      buttons[idx].longPressDetected = true;
    }
  } else {
    // Bouton relâché
    if (buttons[idx].pressedTime != 0) {
      if (!buttons[idx].longPressDetected) {
        buttons[idx].shortPressEventPending = true;
      }
    }
    buttons[idx].pressedTime = 0;
  }
}

bool wasShortPressed(uint8_t idx) {
  if (buttons[idx].shortPressEventPending) {
    buttons[idx].shortPressEventPending = false;
    return true;
  }
  return false;
}

bool wasLongPressed(uint8_t idx) {
  if (buttons[idx].longPressEventFired) {
    buttons[idx].longPressEventFired = false;
    buttons[idx].longPressHasBeenFired = true;
    return true;
  }
  return false;
}

void readButtons() {
  // Pour les 2 premiers boutons sur b_pins
  for (uint8_t p = 0; p < 2; p++) {
    bool reading = digitalRead(b_pins[p]);
    updateButton(p, reading);
    if (wasShortPressed(p)) {
      Serial.print("Button "); Serial.print(p); Serial.println(" short pressed!");
      onButtonShortPress(p);
    }
    if (wasLongPressed(p)) {
      Serial.print("Button "); Serial.print(p); Serial.println(" long pressed!");
      onButtonLongPress(p);
    }
  }
  // Pour les 6 suivants sur le PCF
  for (uint8_t p = 0; p < 6; p++) {
    bool reading = pcf.digitalRead(p);
    uint8_t idx = p + 2;
    updateButton(idx, reading);
    if (wasShortPressed(idx)) {
      Serial.print("Button "); Serial.print(idx); Serial.println(" short pressed!");
      onButtonShortPress(idx);
    }
    if (wasLongPressed(idx)) {
      Serial.print("Button "); Serial.print(idx); Serial.println(" long pressed!");
      onButtonLongPress(idx);
    }
  }
}

void setupButtons() {
  Wire.setSDA(16);  // GP16
  Wire.setSCL(17);  // GP17
  Wire.begin();
  pcf.begin(0x20); // Initialize PCF8574 at address 0x20 with Wire library
  for (uint8_t p=0; p<8; p++) {
    pcf.pinMode(p, INPUT_PULLUP);
  }
  for (uint8_t p=0; p<2; p++) {
    pinMode(b_pins[p], INPUT_PULLUP);
  }
}
