
#include <RotaryEncoder.h>

class Encoders{
    public:
        const uint8_t PIN_IN1[8] = {3, 27, 25, 23, 15, 5, 19, 21};
        const uint8_t PIN_IN2[8] = {28, 26, 24, 22, 14, 4, 18, 20};
        int positions[8] = {0};

        void  read();
        void  setup();

        // Raw position delta since the last call for this index, no gain
        // curve and no coupling to controls/presets. Used by standalone
        // experiments (e.g. the step sequencer test boot).
        int   readDelta(uint8_t idx);

        // Same, but with the same velocity-based gain curve as read()
        // (turn fast = bigger steps), independent of controls/presets.
        int   readDeltaVarispeed(uint8_t idx, int gainMax = 4);

};

extern Encoders encoders;