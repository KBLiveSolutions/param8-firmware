#include <Arduino.h>
#include "encoders.h"
#include "../core/actions.h"
#include "../core/controls.h"

#include "../view/display.h"

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

      // Paramètres à ajuster selon le ressenti souhaité
      const int gain_min = 1;
      const int gain_max = 24;
      const int dt_min = 20;   // rotation très rapide
      const int dt_max = 200;  // rotation très lente

      // Clamp dt entre dt_min et dt_max
      unsigned long dt_clamped = min(max(dt, dt_min), dt_max);

      // Calcul du gain lissé
      float t = float(dt_clamped - dt_min) / float(dt_max - dt_min);
      // int gain = gain_max - (gain_max - gain_min) * t;

      const float a = 5; // à ajuster
      int gain = int(gain_min + (gain_max - gain_min) * exp(-a * t));
      // positions[i] += delta * gain;

      // // Clamp entre 0 et 127
      // if (positions[i] < 0) positions[i] = 0;
      // if (positions[i] > 127) positions[i] = 127;

      // Serial.print("Encodeur ");
      // Serial.print(i);
      // Serial.print(" : ");
      // Serial.println(positions[i]);
      if (controls.getPreset() > 5) onRelativeEncoderChange(i, delta * gain);
      else onAbsoluteEncoderChange(i, delta * gain);
      lastPos[i] = newPos;
      lastTime[i] = now;
      // if (controls.getPreset() == 7) positions[i] = 64;
    }
  }
  };

void Encoders::setup(){
      for (int i = 0; i < 8; i++) {
    encoder[i] = new RotaryEncoder(PIN_IN1[i], PIN_IN2[i], RotaryEncoder::LatchMode::FOUR0);

    // FOUR3 = 1, // 4 steps, Latch at position 3 only (compatible to older versions)
    // FOUR0 = 2, // 4 steps, Latch at position 0 (reverse wirings)
    // TWO03 = 3  // 2 steps, Latch at position 0 and 3 
  }
}