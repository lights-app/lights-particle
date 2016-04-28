#include "lights.h"
#include "PhotonPWM.h"
#include "math.h"

Channel::Channel(byte r, byte g, byte b) {
    
    pin[0] = r;
    pin[1] = g;
    pin[2] = b;
    
    for (byte i = 0; i < 3; i++) {
        pinMode(pin[i], OUTPUT);
    }
    
}

Lights::Lights() {

    // Initialise PWM pins
    output.initTimers();

}

bool Lights::processColorData(String args) {

	Serial.println("Parsing color data");
    Serial.println("Data length: " + String(args.length()));
    
    // Serial.println(temp[16]);
    
    // If the length is not what we expect it to be, then the message is malformed. Return false.
    if (args.length() == 11 + (channelCount * 3) ){
        Serial.println("Data length checked, parsing data");
        
        channels.lightsConfig = args;
        
        for (byte i = 0; i < channelCount; i++) {
            
            Serial.print("Color channel " + String(i) +  ": ");
            
            if (bitRead(args[1], i) == 1 ){
                
                // Serial.print("Channel " + String(i) + " should be on");
                
                channels.channel[i].isOff = false;
                channels.channel[i].turnOff = false;
                channels.channel[i].turnOn = true;

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
                    
                    Serial.print(String(val) + ", ");
                    
                    channels.channel[i].target[j] = val;
                    // Serial.println(channels.channel[i].target[j]);
                    
                }
            
            } else {
                
                channels.channel[i].turnOff = true;
                channels.channel[i].turnOn = false;
                
            }
            
        }
        
        Serial.println("Interpolation time: " + String(channels.interpolationTime));
        
        channels.targetValueReached = false;
        channels.startTime = millis();

        return true;
    
    } else {
        
        return false;
        
    }

}

bool Lights::processTimerData(String args) {

    if (args.length() == 16) {

        Serial.println("Timer data length checked");

        // Determine which timer to set
        byte timerNumber = args[1] - 1;
        Serial.println("Timer Number: " + String(timerNumber));

        // Set the zero point selector
        timers[timerNumber].zeroPointSelector = args[2] - 1;
        Serial.println("Zero point selector: " + String(timers[timerNumber].zeroPointSelector));

        // If the zero point selector is 0, the timer should be disabled
        (args[2] == 0)? timers[timerNumber].enabled = false : timers[timerNumber].enabled = true;

        // Set the zero point offset by combining the next 3 bytes. Subtract 43.200 to get the correct (possibly negative) value. Multiply by 1000 to convert to milliseconds
        byte values[3] = {args[3] - 1, args[4] - 1, args[5] - 1};
        timers[timerNumber].zeroPointOffset = (combineBytes(values, 3) - 43200) * 1000;
        Serial.println("Zero point offset: " + String(timers[timerNumber].zeroPointOffset));

        // Set the interpolation time. Multiply the value by 100 to convert to milliseconds
        byte values2[3] = {args[6] - 1, args[7] - 1, args[8] - 1};
        timers[timerNumber].interpolationTime = combineBytes(values2, 3) * 100;
        Serial.println("Timer interpolation time: " + String(timers[timerNumber].interpolationTime));

        // Set the mode.
        timers[timerNumber].mode = args[9] - 1;
        Serial.println("Timer mode: " + String(timers[timerNumber].mode));

        int maxVal = (127 * 127) - 1;

        for (byte i = 0; i < 3 * 2; i+= 2) {

            byte values[2] = {args[10 + i] - 1, args[10 + i + 1] - 1};
            timers[timerNumber].value[i] = combineBytes(values, 2);
                    
            // Map the received value to a 16-Bit equivalent
            timers[timerNumber].value[i] = map(timers[timerNumber].value[i], 0, maxVal, 0, 65535);
            Serial.println("Color value: " + String(timers[timerNumber].value[i]));

        }

        return true;

    } else {

        Serial.println("Timer data length incorrect");

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
                int val = channels.channel[i].value[j] + (factor * (channels.channel[i].target[j] - channels.channel[i].value[j]));
                output.analogWrite16GC(channels.channel[i].pin[j], val);
                channels.channel[i].currentValue[j] = val;
                
                // Serial.println(val);
                
            }
            
        }
        
    } else {
        
        channels.targetValueReached = true;
        
        for (byte i = 0; i < channelCount; i++) {
        // Serial.println("Writing out channel " + String(i));
        
        for (byte j = 0; j < 3; j++) {
                
                channels.channel[i].value[j] = channels.channel[i].target[j];
                
            }
            
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