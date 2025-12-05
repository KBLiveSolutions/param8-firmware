#include <Adafruit_PCF8574.h>
extern Adafruit_PCF8574 pcf;

#define NB_BUTTONS 10

extern int b_pins[2]; 

struct ButtonState {
  bool lastState = true; // true = relâché (pull-up)
  bool currentState = true;
  unsigned long pressedTime = 0;
  bool shortPressEventPending = false;
};

extern ButtonState buttons[NB_BUTTONS];

void setupButtons();
void readButtons();
void updateButton(uint8_t idx, bool reading);

bool wasShortPressed(uint8_t idx);
void onShortButtonPress(uint8_t idx);