#include "lights.h"
#include "PhotonPWM.h"
#include "math.h"

Channels::Channel::Channel(byte r, byte g, byte b) {

    pin[0] = r;
    pin[1] = g;
    pin[2] = b;
    
    for (byte i = 0; i < 3; i++) {

        pinMode(pin[i], OUTPUT);

    }
    
}

void Channels::Channel::turnOn() {

    for (byte i = 0; i < 3; i++) {

        // Restore the values saved before turning off
        target[i] = savedValue[i];

    }

}

void Channels::Channel::turnOff() {

    for (byte i = 0; i < 3; i++) {

        // Store the last received value before turning the lights off
        savedValue[i] = value[i];
        target[i] = 0;

    }

}

Lights::Lights() {

    // Initialise PWM pins
    // output.initTimers();

}

bool Lights::processColorData(String args) {

    #ifdef LIGHTS_DEBUG
    	Serial.println("Parsing color data");
        Serial.println("Data length: " + String(args.length()));
    #endif
    
    // Serial.println(temp[16]);
    
    // If the length is not what we expect it to be, then the message is malformed. Return false.
    if (args.length() == 11 + (channelCount * 3) ){

        #ifdef LIGHTS_DEBUG
            Serial.println("Data length checked, parsing data");
        #endif
        
        channels.lightsConfig = "";
        channels.lightsConfig = args;
        
        for (byte i = 0; i < channelCount; i++) {

            #ifdef LIGHTS_DEBUG
                Serial.print("Color channel " + String(i) +  ": ");
            #endif
            
            if (bitRead(args[1], i) == 1 ){

                // Serial.print("Channel " + String(i) + " should be on");

                channels.channel[i].isOff = false;

                // Set interpolation time. Multiply by 100 to get from 0.1s increments to 1ms increments
                byte values[3] = {args[2] - 1, args[3] - 1, args[4] - 1};
                channels.interpolationTime = combineBytes(values, 3) * 100;
                
                for (byte j = 0; j < 3; j++) {

                    // Assign the currentValue to the value so colors transition from the correct color during an active transition
                    channels.channel[i].value[j] = channels.channel[i].currentValue[j];
                    
                    // Color data starts at byte 6, each color is represented by 2 bytes
                    byte pos = (i * 3 * 2) + (j * 2) + 5;
                    // Serial.println(pos);
                    
                    // Multiply the first byte by 127, then add the second byte.
                    // This gives a max value slightly lower than 14-Bits
                    // int high = (args[pos] - 1) * 127;
                    // byte low = args[pos + 1] - 1;
                    byte values[2] = {args[pos] - 1, args[pos + 1] - 1};
                    int val = combineBytes(values, 2);

                    int maxVal = (127 * 127) - 1;
                    
                    // Map the received value to a 16-Bit equivalent
                    val = map(val, 0, maxVal, 0, 65535);

                    #ifdef LIGHTS_DEBUG
                        Serial.print(String(val) + ", ");
                    #endif
                    
                    channels.channel[i].target[j] = val;
                    // Serial.println(channels.channel[i].target[j]);
                    
                }

            } else {

                channels.channel[i].turnOff();
                channels.targetValueReached = false;
                
            }
            
        }
        
        #ifdef LIGHTS_DEBUG
            Serial.println("Interpolation time: " + String(channels.interpolationTime));
        #endif
        
        channels.targetValueReached = false;
        channels.startTime = millis();

        return true;

    } else {

        return false;
        
    }

}

bool Lights::processTimerData(String args) {

    if (args.length() == 16) {

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer data length checked");
        #endif

        // Determine which timer to set
        byte timerNumber = args[1] - 1;

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer Number: " + String(timerNumber));
        #endif

        // Every time a timer is set, reset the elapsed value
        timers[timerNumber].hasElapsed = false;

        // Set the zero point selector
        // Determines which point in time is used as a reference: 0 = timer off; 1 = 12:00; 2 = sunrise; 3 = sunset
        timers[timerNumber].zeroPointSelector = args[2] - 1;

        #ifdef LIGHTS_DEBUG
            Serial.println("Zero point selector: " + String(timers[timerNumber].zeroPointSelector));
        #endif

        // If the zero point selector is 0, the timer should be disabled
        (timers[timerNumber].zeroPointSelector == 0)? timers[timerNumber].enabled = false : timers[timerNumber].enabled = true;
        
        #ifdef LIGHTS_DEBUG
            Serial.println("Timer enabled: " + String(timers[timerNumber].enabled));
        #endif

        // Set the zero point offset by combining the next 3 bytes. Subtract 43.199 to get the correct (possibly negative) value
        byte values[3] = {args[3] - 1, args[4] - 1, args[5] - 1};
        timers[timerNumber].zeroPointOffset = (combineBytes(values, 3) - 43199);

        #ifdef LIGHTS_DEBUG
            Serial.println("Zero point offset: " + String(timers[timerNumber].zeroPointOffset));
        #endif

        int value = 0;

        if (timers[timerNumber].zeroPointSelector == 1) {

            value = 43199 + timers[timerNumber].zeroPointOffset;
            timers[timerNumber].zeroPoint = 43199;

        } else if (timers[timerNumber].zeroPointSelector == 2) {

            value = sunriseZeroPoint + timers[timerNumber].zeroPointOffset;
            timers[timerNumber].zeroPoint = sunriseZeroPoint;

        } else if (timers[timerNumber].zeroPointSelector == 3) {
            value = sunsetZeroPoint + timers[timerNumber].zeroPointOffset;
            timers[timerNumber].zeroPoint = sunsetZeroPoint;

        }

        timers[timerNumber].hour = floor(value / 60 / 60);
        timers[timerNumber].minute = floor((value - (timers[timerNumber].hour * 60 * 60)) / 60);
        timers[timerNumber].second = value - (timers[timerNumber].hour * 60 * 60) - (timers[timerNumber].minute * 60);

        if (timers[timerNumber].enabled) {

            #ifdef LIGHTS_DEBUG
                Serial.println("This timer will run at: ");
                Serial.print(timers[timerNumber].hour);
                Serial.print(":");
                Serial.print(timers[timerNumber].minute);
                Serial.print(":");
                Serial.print(timers[timerNumber].second);
                Serial.println();
            #endif

        } else {

            #ifdef LIGHTS_DEBUG
                Serial.println("The timer has been disabled");
            #endif

        }

        // Set the interpolation time. Multiply the value by 100 to convert to milliseconds
        byte values2[3] = {args[6] - 1, args[7] - 1, args[8] - 1};
        timers[timerNumber].interpolationTime = combineBytes(values2, 3) * 100;
        
        #ifdef LIGHTS_DEBUG
            Serial.println("Timer interpolation time: " + String(timers[timerNumber].interpolationTime));
        #endif

        // Set the mode.
        timers[timerNumber].mode = args[9] - 1;

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer mode: " + String(timers[timerNumber].mode));
        #endif

        int maxVal = (127 * 127) - 1;

        for (byte i = 0; i < 3; i ++) {

            byte pos = i * 2;

            byte values[2] = {(byte)(args[10 + pos] - 1), (byte)(args[10 + pos + 1] - 1)};
            timers[timerNumber].value[i] = combineBytes(values, 2);

            // Map the received value to a 16-Bit equivalent
            timers[timerNumber].value[i] = map(timers[timerNumber].value[i], 0, maxVal, 0, 65535);

            #ifdef LIGHTS_DEBUG
                Serial.println("Color value: " + String(timers[timerNumber].value[i]));
            #endif

        }

        timers[timerNumber].timerConfig = "";
        timers[timerNumber].timerConfig = args;

        return true;

    } else {

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer data length incorrect");
        #endif

        return false;

    }

}

void Lights::interpolateColors() {

    if (millis() < channels.startTime + channels.interpolationTime) {

        double t = millis() - channels.startTime;
        // Serial.println(t);
        double perc = t / (double)channels.interpolationTime;
        
        // Serial.println(perc);
        
        for (byte i = 0; i < channelCount; i++) {
            // Serial.println("Writing out channel " + String(i));

            for (byte j = 0; j < 3; j++) {

                double factor = 0.5 * cos((3.1415 * perc) + 3.1415) + 0.5;
                // Serial.println(factor);
                channels.channel[i].currentValue[j] = channels.channel[i].value[j] + (factor * (channels.channel[i].target[j] - channels.channel[i].value[j]));
                output.analogWrite16GC(channels.channel[i].pin[j], channels.channel[i].currentValue[j]);
                
                // Serial.println(channels.channel[i].currentValue[j]);
                
            }
            
        }
        
    } else {

        // Color interpolation complete, set targetValueReached to true
        channels.targetValueReached = true;
        
        // Set channel.value to channel.target
        for (byte i = 0; i < channelCount; i++) {
        // Serial.println("Writing out channel " + String(i));

            for (byte j = 0; j < 3; j++) {

                channels.channel[i].value[j] = channels.channel[i].target[j];
                
            }
            
        }

        // Save the lights/timer config to EEPROM
        saveConfig();
        
    }

}

void Lights::checkTimers() {

    // Serial.println("Actual time: ");
    // Serial.print(Time.hour());
    // Serial.print(":");
    // Serial.print(Time.minute());
    // Serial.print(":");
    // Serial.print(Time.second());
    // Serial.println();

    for (byte i = 0; i < timerCount; i++) {

        // Serial.println();
        // Serial.println("Timer " + String(i));
        // Serial.print(timers[i].hour);
        // Serial.print(":");
        // Serial.print(timers[i].minute);
        // Serial.print(":");
        // Serial.print(timers[i].second);
        // Serial.println();

        if (timers[i].hour == Time.hour() && timers[i].minute == Time.minute() && timers[i].second == Time.second() && timers[i].enabled && !timers[i].hasElapsed) {

            // Serial.println("===================================");
            // Serial.println("Running timer " + String(i));

            byte channelIndex;
            (i < 4)? channelIndex = 0 : channelIndex = 1;

                // Timer is set up to turn off lights
            if (timers[i].mode == 0) channels.channel[channelIndex].turnOff();

                // Timer is set up to turn on lights with last stored values
            if (timers[i].mode == 1) channels.channel[channelIndex].turnOn();

                // Timer is set up to turn on lights to a specific color
            if(timers[i].mode == 2) {

                for (byte j = 0; j < 3; j++) {

                    // Serial.println(timers[i].value[j]);

                    channels.channel[channelIndex].target[j] = timers[i].value[j];

                    // Serial.println(channels.channel[channelIndex].target[j]);

                }

            }

            channels.interpolationTime = timers[i].interpolationTime;
            channels.targetValueReached = false;
            channels.startTime = millis();
            timers[i].hasElapsed = true;

        }

    }

}

int Lights::combineBytes(byte bytes[], byte amountOfBytes) {
    // Serial.println(amountOfBytes);
    int value = 0;

    for (byte i = 0; i < amountOfBytes; i++) {

        value += bytes[i] * pow(127, (amountOfBytes - 1) - i);
        // Serial.println(value);

    }

    return value;

}

void Lights::updateSunTimes() {

    #ifdef LIGHTS_DEBUG
        Serial.println("Setting sun times");
    #endif

    if (timeLord.SunRise(today)) {

        sunriseHour = today[tl_hour];
        sunriseMinute = today[tl_minute];

                // Calculate and set the zero point for the sunrise. Helps in timer configurations
        sunriseZeroPoint = (sunriseHour * 3600) + (sunriseMinute * 60);

        #ifdef LIGHTS_DEBUG
            Serial.println();
            Serial.print("Sunrise will be at: ");
            Serial.print(sunriseHour);
            Serial.print(":");
            Serial.print(sunriseMinute);
            Serial.println();
            Serial.print("Zero Point for sunrise: ");
            Serial.print(sunriseZeroPoint);
            Serial.println();
        #endif

    }

    if (timeLord.SunSet(today)) {

        sunsetHour = today[tl_hour];
        sunsetMinute = today[tl_minute];

                // Calculate and set the zero point for the sunrise. Helps in timer configurations
        sunsetZeroPoint = (sunsetHour * 3600) + (sunsetMinute * 60);

        #ifdef LIGHTS_DEBUG
            Serial.println();
            Serial.print("Sunset will be at: ");
            Serial.print(sunsetHour);
            Serial.print(":");
            Serial.print(sunsetMinute);
            Serial.println();
            Serial.print("Zero Point for sunset: ");
            Serial.print(sunsetZeroPoint);
            Serial.println();
        #endif

    }

}

void Lights::saveConfig() {

    uint16_t memPos = 0;

    // Write the length of the data we're going to write to memory
    EEPROM.write(memPos, channels.lightsConfig.length());

    #ifdef LIGHTS_DEBUG
        Serial.println("Writing " + String(channels.lightsConfig.length()) + " bytes to EEPROM");
    #endif

    // Increment memory position
    memPos++;

    // Save lights config into Lights.config, timer config will be added after this
    config = "";
    // Save version, must be higher than 1 to prevent null termination issues with Strings
    config += (char)(versionMajor + 1);
    config += (char)(versionMinor + 1);
    config += (char)(versionPatch + 1);

    // Save channelCount, must be higher than 1 to prevent null termination issues with Strings
    config += (char)(channelCount + 1);

    config += (char)channels.lightsConfig.length();
    config += channels.lightsConfig;

    for (byte i = 0; i < channels.lightsConfig.length(); i++) {

        EEPROM.write(memPos, channels.lightsConfig[i]);
        #ifdef LIGHTS_DEBUG
            Serial.println("Byte " + String(i) + " " + channels.lightsConfig[i]);
        #endif

        // Increment memory position
        memPos++;

    }

    #ifdef LIGHTS_DEBUG
        Serial.println("Lights data saved");
    #endif

    for (byte i = 0; i < timerCount; i++) {

        // For each timer, store the config in Lights.config
        config += (char)timers[i].timerConfig.length();
        config += timers[i].timerConfig;

        #ifdef LIGHTS_DEBUG
            Serial.println("Writing " + String(timers[i].timerConfig.length()) + " bytes to EEPROM");
        #endif

        // Write the length of the data we're going to write to memory
        EEPROM.write(memPos, timers[i].timerConfig.length());
        // Increment memory position
        memPos++;

        for (byte j = 0; j < timers[i].timerConfig.length(); j++) {

            EEPROM.write(memPos, timers[i].timerConfig[j]);
            #ifdef LIGHTS_DEBUG
                Serial.println("Byte " + String(j) + " " + timers[i].timerConfig[j]);
            #endif

            // Increment memory position
            memPos++;

        }

    }

    #ifdef LIGHTS_DEBUG
        Serial.println("Timer data saved");

        Serial.println("Lights.config updated");
        Serial.println(config);
    #endif

}

void Lights::loadConfig() {

    // Clear any lights configuration currently stored
    channels.lightsConfig = "";

    uint16_t memPos = 0;

    // Get the amount of bytes we need to read
    byte bytesToRead = EEPROM.read(memPos);
    #ifdef LIGHTS_DEBUG
        Serial.println("Reading " + String(bytesToRead) + " bytes from EEPROM");
    #endif

    // Increment memory position
    memPos++;

    // Read X amount of bytes from EEPROM to load lights config
    for(byte i = 0; i < bytesToRead; i++) {
        // Store lights config byte-for-byte
        channels.lightsConfig += (char)EEPROM.read(memPos);
        #ifdef LIGHTS_DEBUG
            Serial.println("Byte " + String(i) + " " + String(EEPROM.read(memPos)));
        #endif

        // Increment memory position
        memPos++;

    }

    #ifdef LIGHTS_DEBUG
        Serial.println("Lights config loaded: " + channels.lightsConfig);
    #endif

    processColorData(channels.lightsConfig);

    // Load lights config into Lights.config, timer config will be added after this
    config = "";
    config += (char)versionMajor;
    config += (char)versionMinor;
    config += (char)versionPatch;

    config += (char)bytesToRead;
    config += channels.lightsConfig;

    // For every timer read X amount of bytes from EEPROM to load timer config
    for (byte i = 0; i < timerCount; i++) {

        // Clear any timer configuration currently stored
        timers[i].timerConfig = "";

        // Get the amount of bytes we need to read
        bytesToRead = EEPROM.read(memPos);
        #ifdef LIGHTS_DEBUG
            Serial.println("Reading " + String(bytesToRead) + " bytes from EEPROM============================");
        #endif

        // Increment memory position
        memPos++;

        for (byte j = 0; j < bytesToRead; j++) {

            // Store timer config byte-for-byte
            timers[i].timerConfig += (char)EEPROM.read(memPos);
            #ifdef LIGHTS_DEBUG
                Serial.println("Byte " + String(j) + " " + String(EEPROM.read(memPos)));
            #endif

            // Increment memory position
            memPos++;

        }

        processTimerData(timers[i].timerConfig);

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer " + String(i) + " config loaded: " + timers[i].timerConfig);
        #endif

        // For each timer, store the config in Lights.config
        config += (char)bytesToRead;
        config += timers[i].timerConfig;

    }

    Serial.println("Lights.config loaded");

}