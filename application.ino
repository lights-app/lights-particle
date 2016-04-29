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

    WiFi.on();

    Particle.function("lights", parseCommand);
    Particle.variable("lightsConfig", lights.channels.lightsConfig);
    Serial.begin(9600);

    // Check for daylight savings time
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +2 : +1);

    // Request time synchronization from the Particle Cloud
    Particle.syncTime();

    lights.longitude = 4.3571;
    lights.latitude = 52.0116;

    // Set timezone for timeLord (in minutes).
    lights.timeLord.TimeZone(isDST(Time.day(), Time.month(), Time.weekday())? 2 * 60 : 1 * 60);
    lights.timeLord.Position(lights.latitude, lights.longitude);
    lights.updateSunTimes();

    lights.today[0] = 0;
    lights.today[1] = 0;
    lights.today[2] = 12;
    lights.today[3] = Time.day();
    lights.today[4] = Time.month();
    lights.today[5] = Time.year();

    // Initialise the Photon's 16-Bit timers
    lights.output.initTimers();

}

void loop() {

    if (!lights.channels.targetValueReached) {

        lights.interpolateColors();
        
    }

    if (millis() - lastSyncTime > SYNC_TIME_INTERVAL) {

        // Check for daylight savings time
        // Do this every hour to ensure Photon is not running behind/fast the first day after DST
        Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +2 : +1);
        
        // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        lastSyncTime = millis();

    }

    // Execute things every second
    if (millis() % 1000 == 0) {
        lights.checkTimers();

        // Serial.print(Time.hour());
        // Serial.print(":");
        // Serial.print(Time.minute());
        // Serial.print(":");
        // Serial.print(Time.second());
        // Serial.println();

        // Execute things once a day at midnight
        if (Time.hour() == 0 && Time.minute() == 0 && Time.second() == 0) {

            lights.updateSunTimes();

            // Reset all timers
            for (byte i = 0; i < lights.timerCount; i++) {

                lights.timers[i].hasElapsed = false;

            }

        }

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

    if (args[0] == 's') {

        lights.updateSunTimes();

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