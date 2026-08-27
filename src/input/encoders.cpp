#include <Arduino.h>
#include "encoders.h"
#include "../core/actions.h"
#include "../core/controls.h"

#include "../view/display.h"

RotaryEncoder* encoder[8];
Encoders encoders;

void encoderISR0() { encoder[0]->tick(); }
void encoderISR1() { encoder[1]->tick(); }
void encoderISR2() { encoder[2]->tick(); }
void encoderISR3() { encoder[3]->tick(); }
void encoderISR4() { encoder[4]->tick(); }
void encoderISR5() { encoder[5]->tick(); }
void encoderISR6() { encoder[6]->tick(); }
void encoderISR7() { encoder[7]->tick(); }

typedef void (*ISRFunc)();
static const ISRFunc encoderISRs[8] = {
    encoderISR0, encoderISR1, encoderISR2, encoderISR3,
    encoderISR4, encoderISR5, encoderISR6, encoderISR7
};

void  Encoders::read(){
  static int lastPos[8] = {0};
  static unsigned long lastTime[8] = {0};

  for (int i = 0; i < 8; i++) {
    int newPos = encoder[i]->getPosition();

    if (lastPos[i] != newPos) {

      int delta = newPos - lastPos[i];
      unsigned long now = millis();
      unsigned long dt = now - lastTime[i];

      bool isRelative = controls.getPreset() > 5;
      const int gain_min = 1;
      const int gain_max = isRelative ? 5 : 4;
      const unsigned long dt_min = 15;
      const unsigned long dt_max = 150;

      unsigned long dt_clamped = min(max(dt, dt_min), dt_max);

      float t = float(dt_clamped - dt_min) / float(dt_max - dt_min);
      int gain = int(gain_min + (gain_max - gain_min) * (1.0f - t * t));

      if (isRelative) onRelativeEncoderChange(i, delta * gain);
      else onAbsoluteEncoderChange(i, delta * gain);
      lastPos[i] = newPos;
      lastTime[i] = now;
    }
  }
  };

void Encoders::setup(){
  for (int i = 0; i < 8; i++) {
    encoder[i] = new RotaryEncoder(PIN_IN1[i], PIN_IN2[i], RotaryEncoder::LatchMode::TWO03);
    attachInterrupt(digitalPinToInterrupt(PIN_IN1[i]), encoderISRs[i], CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_IN2[i]), encoderISRs[i], CHANGE);
  }
}