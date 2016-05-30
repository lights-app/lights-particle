#define LIGHTS_DEBUG

// Enable system threading to ensure main loop continues regardless of cloud connection
// Disable system threading for debugging. Threading enabled messes with Serial printing
#ifndef LIGHTS_DEBUG
    SYSTEM_THREAD(ENABLED);
#endif

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

    // Call WiFi.on() to ensure WiFi is turned on
    WiFi.on();

    // Check Wi-Fi signal strength
    if (WiFi.RSSI() < -60) {

        // If it is low, select the external antenna to check its reception
        WiFi.selectAntenna(ANT_EXTERNAL);

        // If it is also low, set antenna to auto and let it dynamically switch between them
        if (WiFi.RSSI() < -60) {

            WiFi.selectAntenna(ANT_AUTO);

        }

    }

    Particle.function("lights", parseCommand);
    Particle.variable("config", lights.config);
    Serial.begin(9600);

    // Check for daylight savings time
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +2 : +1);

    // Request time synchronization from the Particle Cloud
    Particle.syncTime();

    // Set lights version, must be higher than 1 to prevent null termination issues with Strings
    lights.versionMajor = 1;
    lights.versionMinor = 1;
    lights.versionPatch = 1;

    // Set timezone for timeLord (in minutes).
    lights.timeLord.TimeZone(isDST(Time.day(), Time.month(), Time.weekday())? 2 * 60 : 1 * 60);
    lights.timeLord.Position(52.0116, 4.3571);
    lights.updateSunTimes();

    lights.today[0] = 0;
    lights.today[1] = 0;
    lights.today[2] = 12;
    lights.today[3] = Time.day();
    lights.today[4] = Time.month();
    lights.today[5] = Time.year();

    // Initialise the Photon's 16-Bit timers
    lights.output.initTimers();

    // Load the lights/timer config from EEPROM
    lights.loadConfig();

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

        #ifdef LIGHTS_DEBUG

            Serial.println("Wifi strength: " + String(WiFi.RSSI()));

            Serial.print(Time.hour());
            Serial.print(":");
            Serial.print(Time.minute());
            Serial.print(":");
            Serial.print(Time.second());
            Serial.println();

        #endif

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

        #ifdef LIGHTS_DEBUG
            Serial.println("Received color data");
        #endif
        
        
        if (lights.processColorData(args)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
        
    }
    
    if (args[0] == 't') {

        #ifdef LIGHTS_DEBUG
            Serial.println("Received timer data");
        #endif
        
        if (lights.processTimerData(args)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
    }

    if (args[0] == 's') {

        lights.updateSunTimes();

    }

    if (args[0] == 'w') {

        lights.saveConfig();

    }

    if (args[0] == 'l') {

        lights.loadConfig();
        
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