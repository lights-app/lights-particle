// Enable system threading to ensure main loop continues regardless of cloud connection
SYSTEM_THREAD(ENABLED);
// Set system mode to automatic to ensure Wifi will work automatically
SYSTEM_MODE(AUTOMATIC);


// #include "main.h"
#include "lights.h"
#include "PhotonPWM.h"
#include "math.h"
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

// Create new instance of Lights class
Lights lights;

// Set syncTime interval
#define SYNC_TIME_INTERVAL (60 * 60 * 1000)
unsigned long lastSyncTime = millis();

void setup() {

    Particle.function("lights", parseCommand);
    Particle.variable("lightsConfig", lights.channels.lightsConfig);
    Serial.begin(9600);
    
}

void loop() {

    if (!lights.channels.targetValueReached) {

        lights.interpolateColors();
        
    }

    if (millis() - lastSyncTime > SYNC_TIME_INTERVAL) {

        // Check for daylight savings time
        Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +1 : 0);
    
        // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        lastSyncTime = millis();

    }

}

int parseCommand(String args) {

    Serial.println(args);
    
    if (args[0] == 'c') {

        Serial.println("Received color data");
        
        
        if (lights.processColorData(args)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
        
    }
    
    if (args[0] == 't') {

        Serial.println("Received timer data");
        
        if (lights.processTimerData(args)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
    }
    

    return args[0];

}

bool isDST(int dayOfMonth, int month, int dayOfWeek)
{
    if (month < 3 || month > 11)
    {
        return false;
    }
    if (month > 3 && month < 11)
    {
        return true;
    }
    int previousSunday = dayOfMonth - (dayOfWeek - 1);
    if (month == 3)
    {
        return previousSunday >= 8;
    }
    return previousSunday <= 0;
}