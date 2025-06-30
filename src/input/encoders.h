
#include <RotaryEncoder.h>

class Encoders{
    public:
        const uint8_t PIN_IN1[8] = {3, 27, 25, 23, 15, 5, 19, 21};
        const uint8_t PIN_IN2[8] = {28, 26, 24, 22, 14, 4, 18, 20};
        int positions[8] = {0};

        void  read();
        void  setup();

};

extern Encoders encoders;