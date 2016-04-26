#include "main.h"

Channel::Channel(byte r, byte g, byte b) {
    
    pin[0] = r;
    pin[1] = g;
    pin[2] = b;
    
    for (byte i = 0; i < 3; i++) {
        pinMode(pin[i], OUTPUT);
    }
    
}

Channels::Channels() {
    
    
    
}