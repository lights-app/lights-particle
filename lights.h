#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

#include "application.h"
#include "PhotonPWM.h"

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
    
        // Channels();
        String lightsConfig = "";
        
        unsigned int interpolationTime;
        unsigned int startTime;
        bool targetValueReached = false;
        
        Channel channel[2] = {Channel(D0, D1, D2), Channel(WKP, RX, TX)};
    
};

class LightsTimer {

private:


public:

    // Timer configuration in same format as Lights communication protocol. Used to return timer configuration to user
    String timerConfig;

    // Determines if the timer is enabled
    bool enabled;

    // Determines if the timer has elapsed. Reset every 24 hours
    bool hasElapsed;

    // Determines which point in time is used as a reference: 0 = timer off; 1 = 12:00; 2 = sunrise; 3 = sunset
    byte zeroPointSelector;

    // Offset from zero point. Each value represents 1 second. -43.200 or +43.200 relative to zero point (86400 seconds per day)
    // Since we can't send negative values, 43.200 will be subtracted from the received value
    int zeroPointOffset;

    // Interpolation time used when timer runs
    unsigned int interpolationTime;

    // Timer mode: 0 = turns off the lights, 1 = turns on the lights with the last stored color values, 2 = turns on lights with color values stored in the timer
    byte mode;

    // The hour at which the timer should start
    byte hour;

    // The minute at which the timer should start
    byte minute;

    // The second at which the timer should start
    byte second;

    // Color value the lights should be when timer runs. Not used when 
    uint16_t value[3];

};

class Lights {

private:


public:

    Lights();

    const static byte channelCount = 2;
    const static byte timerCount = 8;
    Channels channels;
    LightsTimer timers[timerCount];
    PhotonPWM output;

    double longitude;
    double latitude;
    byte today[6];
    byte sunriseHour;
    byte sunriseMinute;
    byte sunsetHour;
    byte sunsetMinute;

    bool processColorData(String args);
    bool processTimerData(String args);
    void interpolateColors();
    int combineBytes(byte bytes[], byte amountOfBytes);

};