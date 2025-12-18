#include "buttons.h"
#include "../core/actions.h"

int b_pins[2] = {2, 1}; // GPIO pins for buttons
Adafruit_PCF8574 pcf;

ButtonState buttons[NB_BUTTONS];


void updateButton(uint8_t idx, bool reading) {
  buttons[idx].lastState = buttons[idx].currentState;
  buttons[idx].currentState = reading;
  unsigned long now = millis();

  // Vérifier si l'état a changé
  if (buttons[idx].lastState != buttons[idx].currentState) {
    if (!buttons[idx].currentState) { // Bouton appuyé (active bas)
      if (buttons[idx].pressedTime == 0) {
        onButtonPressed(idx);
        // Vient d'être pressé
        buttons[idx].pressedTime = now;
        buttons[idx].shortPressEventPending = false;
      }
    } else {
      onButtonReleased(idx);
      // Bouton relâché
      if (buttons[idx].pressedTime != 0) {
        buttons[idx].shortPressEventPending = true;
      }
      buttons[idx].pressedTime = 0;
    }
  }
}

bool wasShortPressed(uint8_t idx) {
  if (buttons[idx].shortPressEventPending) {
    buttons[idx].shortPressEventPending = false;
    return true;
  }
  return false;
}

void readButtons() {
  // Pour les 2 premiers boutons sur b_pins
  for (uint8_t p = 0; p < 2; p++) {
    bool reading = digitalRead(b_pins[p]);
    // updateButton(p, reading);

    // Gestion spéciale pour Shift (0) et Latch (1)
    if (p == 0) { // Shift
      if (!reading) { // appuyé (active bas)
        if(!shiftPressed)onShiftPress();
      } 
      else {
        if(shiftPressed) onShiftRelease();
      }
    } 
    else if (p == 1) { // Latch
      if (!reading ) { // appuyé (active bas)
       if(!latchPressed) onLatchPress();
      } 
      else {
        if (latchPressed) onLatchRelease();
      }
    }

    // if (wasShortPressed(p)) {
    //   Serial.print("Button "); Serial.print(p); Serial.println(" short pressed!");
    //   onButtonShortPress(p);
    // }
  }
  // Pour les 6 suivants sur le PCF
  for (uint8_t p = 0; p < 8; p++) {
    bool reading = pcf.digitalRead(p);
    uint8_t idx = p;
    updateButton(idx, reading);
    // if (wasShortPressed(idx)) {
    //   Serial.print("Button "); Serial.print(idx - 1); Serial.println(" short pressed!");
    //   onButtonShortPress(idx);
    // }
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
