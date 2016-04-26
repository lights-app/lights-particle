#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

#include "application.h"

// byte photonPinMap[][3] = {{D0, D1, D2}, {WKP, RX, TX}};

class Channel {
  private:
    
  
  public:
    Channel(byte, byte, byte);
    
    byte pin[3];
    uint16_t value[3] = {0, 0, 0};
    uint16_t target[3] = {0, 0, 0};
    uint16_t currentValue[3] = {0, 0, 0};
    
    const int off[3] = {0, 0, 0};
    bool turnOff;
    bool turnOn;
    bool isOff;
  
};

class Channels {
    
    private:
        
    
    public:
    
        Channels();
        
        const static byte channelCount = 2;
        
        unsigned long interpolationTime;
        unsigned long startTime;
        bool targetValueReached = false;
        
        Channel channel[channelCount] = {Channel(D0, D1, D2), Channel(WKP, RX, TX)};
    
};