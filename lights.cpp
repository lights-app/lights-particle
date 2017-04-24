#include "lights.h"
#include "PhotonPWM.h"
#include "math.h"

#define LIGHTS_DEBUG

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
        isOff = false;

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
            Serial.println(channels.targetValueReached);
        #endif

        if (!channels.targetValueReached) {

            #ifdef LIGHTS_DEBUG
                Serial.println("New data received before end of transition, publishing previous data");
            #endif

            // We save the config, but do not write to EEPROM since we're getting new data
            saveConfig(false);

            // publish change event if new data was received before the target value of the previous transition was reached
            // This keeps all connected users up to date with the latest color information
            Particle.publish("configChanged", config, 1);

        }
        
        channels.lightsConfig = "";
        channels.lightsConfig = args;
        
        for (byte i = 0; i < channelCount; i++) {

            #ifdef LIGHTS_DEBUG
                Serial.print("Color channel " + String(i) +  ": ");
            #endif

            // Channel on/off state is shifted 1 bit since we can't send 0
            // Channel states are saved in a single byte.
            // 0 and >127 can't be sent, thus a maximum of 6 channel states are sent
            // 01111110 would turn all channels on. Note that the first and last bit are unused
            // due to the 1-127 limit
            if (bitRead(args[1], i + 1) == 1 ){

                #ifdef LIGHTS_DEBUG
                    Serial.println("Channel " + String(i) + " should be on");
                #endif

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

                #ifdef LIGHTS_DEBUG
                    Serial.println("Channel " + String(i) + " is off");
                #endif
                
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

bool Lights::processTimerData(String args, bool saveAfterProcessing) {

    if (args.length() == 16) {

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer data length checked");
        #endif

        // Determine which timer to set
        byte timerNumber = args[1] - 1;

        // If the timer number is out of range, return false
        if (timerNumber >= timerCount) {

            #ifdef LIGHTS_DEBUG
                Serial.println("Timer Number: " + String(timerNumber) + " out of range");
            #endif

            return false;

        }

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer Number: " + String(timerNumber));
        #endif

        // Every time a timer is set, reset the elapsed value
        timers[timerNumber].hasElapsed = false;

        // Set the zero point selector
        // Determines which point in time is used as a reference: 0 = timer off; 1 = 12:00; 2 = sunrise; 3 = sunset
        timers[timerNumber].zeroPointSelector = args[2] - 1;

        // If the zero point selector is out of range, return false
        if (timers[timerNumber].zeroPointSelector > 3) {

            #ifdef LIGHTS_DEBUG
                Serial.println("zeroPointSelector: " + String(timers[timerNumber].zeroPointSelector) + " out of range");
            #endif

            return false;

        }

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

        // If the mode is out of range, return false
        if (timers[timerNumber].mode > 2) {

            #ifdef LIGHTS_DEBUG
                Serial.println("mode: " + String(timers[timerNumber].mode) + " out of range");
            #endif

            return false;

        }

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

        if (saveAfterProcessing) {

            // Save the lights/timer config to EEPROM
            saveConfig();

        }

        #ifdef LIGHTS_DEBUG
            Serial.println("New timer data saved, publishing latest data");
        #endif

        // Publish change event to the cloud after the new config has been saved
        Particle.publish("configChanged", config, 1);

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
        
        // Set channel.value to channel.target
        for (byte i = 0; i < channelCount; i++) {
        // Serial.println("Writing out channel " + String(i));

            for (byte j = 0; j < 3; j++) {

                channels.channel[i].value[j] = channels.channel[i].target[j];
                
            }
            
        }

        // Save the lights/timer config to EEPROM
        saveConfig();

        // Color interpolation complete, set targetValueReached to true
        channels.targetValueReached = true;

        #ifdef LIGHTS_DEBUG
            Serial.println("Target value reached, publishing latest color data");
        #endif

        // Publish change event to the cloud after the new config has been saved
        Particle.publish("configChanged", config, 1);
        
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

void Lights::resetLights() {

    // Lights off, 700 ms interpolation time
    channels.lightsConfig = "d";

}

void Lights::resetTimer(byte timerNumber) {

    if(timerNumber < timerCount){

        #ifdef LIGHTS_DEBUG
            Serial.println("Resetting timer " + String(timerNumber));
        #endif

        // Add one to the letter t
        timers[timerNumber].timerConfig = (char)('t' + 1);
        timers[timerNumber].timerConfig += (char)(timerNumber + 1);
        timers[timerNumber].timerConfig += "3";

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer length: " + String(timers[timerNumber].timerConfig.length()));
        #endif

        // for (byte j = 0; j < 14; j++) {
        //     timers[timerNumber].timerConfig += (char)1;
        // }

        saveConfig();

    } else {

        #ifdef LIGHTS_DEBUG
            Serial.println("Can't reset timer" + String(timerNumber) + ". Out of bounds");
        #endif
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

int Lights::splitBytes(int num, byte amountOfBytes, byte* bytes) {
    // Serial.println(amountOfBytes);
    int value = 0;

    for (byte i = 0; i < amountOfBytes; i++) {

        value += bytes[i] * pow(127, (amountOfBytes - 1) - i);

        bytes[i] = floor(num / pow(127, (amountOfBytes - 1) - i));

        num -= bytes[i] * pow(127, (amountOfBytes - 1) - i);
        // Serial.println(value);

    }

    return 1;

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

// writeToEEPROM is set to true as default
void Lights::saveConfig(bool writeToEEPROM) {

    #ifdef LIGHTS_DEBUG
        Serial.println("Writing " + String(channels.lightsConfig.length()) + " bytes to EEPROM");
    #endif

    String tempConf = "";
    // Save lights config into Lights.config, timer config will be added after this
    tempConf = "";

    // Save the content length

    // Save version, must be higher than 1 to prevent null termination issues with Strings
    tempConf += (char)(versionMajor + 1);
    tempConf += (char)(versionMinor + 1);
    tempConf += (char)(versionPatch + 1);

    // Save antenna mode. 0 = auto, 1 = internal, 2 = external
    tempConf += (char)(antennaMode + 1);

    // Save channelCount, must be higher than 1 to prevent null termination issues with Strings
    tempConf += (char)(channelCount + 1);

    tempConf += (char)(channels.lightsConfig.length() + 1);

    // The lightsConfig already has +1 for each byte, so we can just store it directly
    tempConf += channels.lightsConfig;

    #ifdef LIGHTS_DEBUG
        Serial.println();
    #endif

    for (byte i = 0; i < timerCount; i++) {

        #ifdef LIGHTS_DEBUG
            Serial.println("Writing " + String(timers[i].timerConfig.length()) + " bytes to EEPROM");
        #endif

        // For each timer, store the config in Lights.config
        tempConf += (char)(timers[i].timerConfig.length() + 1);
        // The timerConfig already has +1 for each byte, so we can just store it directly
        tempConf += timers[i].timerConfig;

    }

    //reset config
    config = "";

    // Save the total content length as the first byte
    byte bytes[2];
    splitBytes(tempConf.length(), 2, bytes);
    config += (char)(bytes[0] + 1);
    config += (char)(bytes[1] + 1);

    // Then store the complete config after that 
    config += tempConf;

    // Write out the config to EEPROM
    if (writeToEEPROM) {

        for (uint16_t i = 0; i < config.length(); i++) {

            EEPROM.write(i, config[i]);

        }

    }

    #ifdef LIGHTS_DEBUG
        Serial.println("Lights config written to EEPROM: " + config);
    #endif

}

void Lights::loadConfig() {

    // Clear any lights configuration currently stored
    channels.lightsConfig = "";

    // Load EEPROM config into Lights.config, timer config will be added after this
    config = "";
    byte configLength[2];
    configLength[0] = (char)(EEPROM.read(0) - 1);
    configLength[1] = (char)(EEPROM.read(1) - 1);
    // The content length stored in these two bytes does not include these bytes,
    // so we add 2 to compensate
    uint16_t eepromBytesToRead = combineBytes(configLength, 2) + 2;

    if (eepromBytesToRead > 0) {

        for (uint16_t i = 0; i < eepromBytesToRead; i++) {

            config += (char)EEPROM.read(i);

        }

    }

    // Ensure that the config holds the actual version number
    // This may have changed during a firmware update, meaning the config saved in EEPROM
    // would not reflect the actual version number
    config[2] = (char)(versionMajor + 1);
    config[3] = (char)(versionMinor + 1);
    config[4] = (char)(versionPatch + 1);

    // Load the current antenna mode from EEPROM into Lights object
    antennaMode = (char)(config[5] - 1);

    // The data we need starts at byte 7
    uint16_t memPos = 7;

    // Get the amount of bytes we need to read
    uint16_t bytesToRead = EEPROM.read(memPos);
    #ifdef LIGHTS_DEBUG
        Serial.println("Reading " + String(bytesToRead) + " bytes from EEPROM");
    #endif

    // Increment memory position
    memPos++;

    // Read X amount of bytes from EEPROM to load lights config. We did +1 for bytesToRead when storing it, so subtract 1
    for(byte i = 0; i < bytesToRead - 1; i++) {
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

    // For every timer read X amount of bytes from EEPROM to load timer config
    for (byte i = 0; i < timerCount; i++) {

        // Clear any timer configuration currently stored
        timers[i].timerConfig = "";

        // Get the amount of bytes we need to read
        bytesToRead = EEPROM.read(memPos);

        #ifdef LIGHTS_DEBUG
            Serial.println("Reading " + String(bytesToRead) + " bytes from EEPROM============================");
        #endif

        // Check data length. If it is lower than 16, the timer has not been configured yet. We did +1 for bytesToRead when storing it, so subtract 1
        if (bytesToRead - 1 < 16) {

            #ifdef LIGHTS_DEBUG
                Serial.println(String(bytesToRead) + " length incorrect, resetting timer to 0 config");
            #endif

                // Load a 0 (1, since we subtract 1 later) config into timerConfig
                resetTimer(i);

        } else {

            // Increment memory position
            memPos++;

            // We did +1 for bytesToRead when storing it, so subtract 1
            for (byte j = 0; j < bytesToRead - 1; j++) {

                // Store timer config byte-for-byte
                timers[i].timerConfig += (char)EEPROM.read(memPos);

                #ifdef LIGHTS_DEBUG
                    Serial.println("Byte " + String(j) + " " + String(EEPROM.read(memPos)));
                #endif

                // Increment memory position
                memPos++;

            }
        }

        // If the timer processor returns false, reset the timer to a 0 config
        if (!processTimerData(timers[i].timerConfig, false)) {

            #ifdef LIGHTS_DEBUG
                Serial.println("Error processing timer, resetting to 0 config");
            #endif

            resetTimer(i);

        }

        #ifdef LIGHTS_DEBUG
            Serial.println("Timer " + String(i) + " config loaded: " + timers[i].timerConfig);
        #endif

    }

    Serial.println("Lights.config loaded");
    saveConfig(false);

    // Finally, process the color data to return lights to last saved setting
    processColorData(channels.lightsConfig);

}

void Lights::serialPacketReceived() {

    while(Serial.available() > 0){

        char recv = Serial.read();

        // Received start flag, check if buffer is full. If so set PWMs
        if (recv == 0xFF) {

            if (serialByteCount == channelCount * 3 * 2) {


                for (byte i = 0; i < channelCount; i++) {

                    for (byte j = 0; j < 3 * 2; j += 2){

                        byte pos = (i * 3 * 2) + j;
                        uint16_t val = ambilightBuffer[pos];
                        val = val<<8;
                        val |= ambilightBuffer[pos + 1];

                        output.analogWrite16(channels.channel[i].pin[j/2], val);

                    }

                }

                serialByteCount = 0;

            } else {

                USBSerial1.println("incorrect data len " + String(serialByteCount));
                serialByteCount = 0;

            }

        // Received init flag, echo current Lights values
        } else if (recv == 0xFE) {


        // No flag received, write to buffer
        } else {

            ambilightBuffer[serialByteCount] = recv;
            serialByteCount++;

        }

        if (millis() % 10000 == 0) {

            Particle.process();

        }

    }

}