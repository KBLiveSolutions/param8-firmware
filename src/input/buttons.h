#include <Adafruit_PCF8574.h>
extern Adafruit_PCF8574 pcf;

#define NB_BUTTONS 8

extern int b_pins[2]; 
extern const uint16_t longPressDuration;

struct ButtonState {
  bool lastState = true; // true = relâché (pull-up)
  bool currentState = true;
  unsigned long pressedTime = 0;
  bool longPressEventFired = false;
  bool longPressDetected = false;
  bool shortPressEventPending = false;
  bool longPressHasBeenFired = false;
};

extern ButtonState buttons[NB_BUTTONS];

void setupButtons();
void readButtons();
void onButtonPress(uint8_t);
void updateButton(uint8_t idx, bool reading);

bool wasShortPressed(uint8_t idx);
bool wasLongPressed(uint8_t idx);
void onShortButtonPress(uint8_t idx);
void onLongButtonPress(uint8_t idx);