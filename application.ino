// #define LIGHTS_DEBUG
// #define LIGHTS_DETAILS

// Enable system threading to ensure main loop continues regardless of cloud connection
// Disable system threading for debugging. Threading enabled messes with Serial printing
// #ifndef LIGHTS_DEBUG
//     SYSTEM_THREAD(ENABLED);
// #endif

SYSTEM_THREAD(DISABLED);

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
    // if (WiFi.RSSI() < -60) {

    //     // If it is low, select the external antenna to check its reception
    //     WiFi.selectAntenna(ANT_EXTERNAL);

    //     // If it is also low, set antenna to auto and let it dynamically switch between them
    //     if (WiFi.RSSI() < -60) {

    //         WiFi.selectAntenna(ANT_AUTO);

    //     }

    // }

    Particle.function("lights", parseCommand);
    Particle.variable("config", lights.config);
    Serial.begin(115200);

    // Check for daylight savings time
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +2 : +1);

    // Request time synchronization from the Particle Cloud
    Particle.syncTime();

    // Set lights version. Can't be higher than 126 (1 will be automatically added)
    lights.versionMajor = 0;
    lights.versionMinor = 2;
    lights.versionPatch = 2;

    // Set timezone for timeLord (in minutes).
    lights.timeLord.TimeZone(isDST(Time.day(), Time.month(), Time.weekday())? 2 * 60 : 1 * 60);
    lights.timeLord.Position(52.0116, 4.3571);

    #ifdef LIGHTS_DEBUG

        Serial.println("DST: " + String(isDST(Time.day(), Time.month(), Time.weekday()) ));
        Serial.println("Wifi strength: " + String(WiFi.RSSI()));

        Serial.print(Time.year());
        Serial.print("/");
        Serial.print(Time.month());
        Serial.print("/");
        Serial.print(Time.day());
        Serial.println();

        Serial.print(Time.hour());
        Serial.print(":");
        Serial.print(Time.minute());
        Serial.print(":");
        Serial.print(Time.second());
        Serial.println();

    #endif

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

    // After config is loaded, assign antenna value
    if (lights.antennaMode == 0) {

        WiFi.selectAntenna(ANT_AUTO);

    } else if (lights.antennaMode == 1) {

        WiFi.selectAntenna(ANT_INTERNAL);

    } else if (lights.antennaMode == 2) {

        WiFi.selectAntenna(ANT_EXTERNAL);

    } else {

        // If the value in the config does not conform to the modes, set it to auto
        WiFi.selectAntenna(ANT_AUTO);
        lights.antennaMode = 0;
    }

}

void loop() {

    if (!lights.channels.targetValueReached) {

        lights.interpolateColors();
        
    }

    if (millis() - lastSyncTime > SYNC_TIME_INTERVAL) {

        // Check for daylight savings time
        // Do this every hour to ensure Photon is not running behind/fast the first day after DST
        Time.zone(isDST(Time.day(), Time.month(), Time.weekday())? +2 : +1);

        // Update sun times every hour. The first update may not work correctly 
        lights.updateSunTimes();
        
        // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        lastSyncTime = millis();

    }

    // Execute things every second
    if (millis() % 1000 == 0) {
        
        lights.checkTimers();

        #ifdef LIGHTS_DETAILS

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

    if (Serial.available() > 0) {

        lights.serialPacketReceived();
    }

}

int parseCommand(String args) {

    Serial.println(args);
    
    if (args[0] - 1 == 'c') {

        #ifdef LIGHTS_DEBUG
            Serial.println("Received color data");
            Serial.println(lights.channels.targetValueReached);
        #endif
        
        if (lights.processColorData(args)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
        
    }
    
    if (args[0] - 1 == 't') {

        #ifdef LIGHTS_DEBUG
            Serial.println("Received timer data");
        #endif
        
        if (lights.processTimerData(args, true)) {

            return 200;
            
        } else {

            return 400;
            
        }
        
    }

    if (args[0] - 1 == 's') {

        lights.updateSunTimes();

    }

    if (args[0] - 1 == 'w') {

        lights.saveConfig();

    }

    if (args[0] - 1 == 'l') {

        lights.loadConfig();
        
    }

    // Set antenna. This settings is remembered after reboot, so we only need to set it once
    if (args[0] - 1 == 'a') {

        if (args[1] - 1 == 0) {

            #ifdef LIGHTS_DEBUG
                Serial.print("Setting antenna to AUTO");
            #endif

            WiFi.selectAntenna(ANT_AUTO);

            lights.antennaMode = 0;
            lights.saveConfig();

            #ifdef LIGHTS_DEBUG
                Serial.println("Auto");
            #endif

            return 200;

        }

        if (args[1] - 1 == 1) {

            #ifdef LIGHTS_DEBUG
                Serial.print("Setting antenna to INTERNAL");
            #endif

            WiFi.selectAntenna(ANT_INTERNAL);

            lights.antennaMode = 1;
            lights.saveConfig();

            #ifdef LIGHTS_DEBUG
                Serial.println("Internal");
            #endif
                
            return 200;

        }

        if (args[1] - 1 == 2) {

            #ifdef LIGHTS_DEBUG
                Serial.print("Setting antenna to EXTERNAL");
            #endif

            WiFi.selectAntenna(ANT_EXTERNAL);

            lights.antennaMode = 2;
            lights.saveConfig();

            #ifdef LIGHTS_DEBUG
                Serial.println("External");
            #endif
                
            return 200;

        }

        return 400;

    }

    // Reset a specific timer
    if (args[0] - 1 == 'r') {

        lights.resetTimer(args[1] - 1);
        
    }

    // Reset the whole memory
    if (args[0] - 1 == 'z') {

        Serial.println("Resetting memory");

        for (uint16_t i = 0; i < EEPROM.length(); i++) {

            EEPROM.write(i, 1);

        }

        lights.resetLights();

        for (byte i = 0; i < lights.timerCount; i++) {

            lights.resetTimer(i);
            // Process the reset timer data. 
            // It does not need to be saved since we already stored the reset data in EEPROM when we reset the timer
            lights.processTimerData(lights.timers[i].timerConfig, false);

        }

        lights.saveConfig();

        Serial.println("Memory reset complete");
        
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