// Arduino mock state implementation
#include "Arduino.h"

namespace arduino_mock {
    unsigned long current_millis = 0;
    int pin_states[64] = {0};
    int analog_values[16] = {0};

    void reset() {
        current_millis = 0;
        for (int i = 0; i < 64; i++) pin_states[i] = 0;
        for (int i = 0; i < 16; i++) analog_values[i] = 0;
    }
}

// Global objects
HardwareSerial Serial;
HardwareSerial Serial1;
TwoWire Wire;
SPIClass SPI;
