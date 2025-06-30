#include <Arduino.h>
#include "encoders.h"
#include "../core/actions.h"


RotaryEncoder* encoder[8];
Encoders encoders;

void  Encoders::read(){
      static int lastPos[8] = {0};
    static unsigned long lastTime[8] = {0};

  for (int i = 0; i < 8; i++) {
    encoder[i]->tick();
    int newPos = encoder[i]->getPosition();

    // Si la position a changé
    if (lastPos[i] != newPos) {
      int delta = newPos - lastPos[i];
      unsigned long now = millis();
      unsigned long dt = now - lastTime[i];

      // Si l'encodeur est tourné rapidement (<50ms), on augmente le pas
      int gain = 1;
      if (dt < 50) gain = 4;
      else if (dt < 100) gain = 2;

      positions[i] += delta * gain;

      // Clamp entre 0 et 127
      if (positions[i] < 0) positions[i] = 0;
      if (positions[i] > 127) positions[i] = 127;

      Serial.print("Encodeur ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.println(positions[i]);
      onEncoderChange(i, positions[i]);
      lastPos[i] = newPos;
      lastTime[i] = now;
    }
  }
  };

void Encoders::setup(){
      for (int i = 0; i < 8; i++) {
    encoder[i] = new RotaryEncoder(PIN_IN2[i], PIN_IN1[i], RotaryEncoder::LatchMode::TWO03);

    // FOUR3 = 1, // 4 steps, Latch at position 3 only (compatible to older versions)
    // FOUR0 = 2, // 4 steps, Latch at position 0 (reverse wirings)
    // TWO03 = 3  // 2 steps, Latch at position 0 and 3 
  }
}