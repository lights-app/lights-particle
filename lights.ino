
#include "main.h"
#include "PhotonPWM.h"
#include "math.h"
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

Channels channels;
String lightsConfig = "";

//Create new instance of PhotonPWM
PhotonPWM photonPWM;

void setup() {
    
    Particle.function("lights", parseCommand);
    Particle.variable("lightsConfig", lightsConfig);
    Serial.begin(9600);
    
    photonPWM.initTimers();
    
}

void loop() {
    
    if (!channels.targetValueReached) {
        
        interpolateColors();
        
    }

}

int parseCommand(String args) {
    
    Serial.println(args);
    
    if (args[0] == 'c') {
        
        Serial.println("Received color data");
        
        if (parseColors(args)) {
            
            return 200;
            
        } else {
            
            return 400;
            
        }
        
        
    }
    
    if (args[0] == 't') {
        
        parseTimer(args);
        
    }
    
  
  return args[0];
  
}

bool parseColors(String args) {
    Serial.println("Parsing color data");
    Serial.println("Data length: " + String(args.length()));
    
    // Serial.println(temp[16]);
    
    // If the lenght is not what we expect it to be, then the message is malformed. Return false.
    if (args.length() == 17){
        Serial.println("Data length checked, parsing data");
        
        lightsConfig = args;
        
        for (byte i = 0; i < channels.channelCount; i++) {
            
            Serial.print("Color channel " + String(i) +  ": ");
            
            if (bitRead(args[1], i) == 1 ){
                
                // Serial.print("Channel " + String(i) + " should be on");
                
                channels.channel[i].isOff = false;
                channels.channel[i].turnOff = false;
                channels.channel[i].turnOn = true;
                
                for (byte j = 0; j < 3; j++) {
                    
                    // Assign the currentValue to the value so colors transition from the correct color during an active transition
                    channels.channel[i].value[j] = channels.channel[i].currentValue[j];
                    
                    // Color data starts at byte 3, each color is represented by 2 bytes
                    byte pos = (i * 3 * 2) + (j * 2) + 2;
                    // Serial.println(pos);
                    
                    // Multiply the first byte by 127, then add the second byte.
                    // This gives a max value slightly lower than 14-Bits
                    int high = (args[pos] - 1) * 127;
                    byte low = args[pos + 1] - 1;
                    int val = high + low;
                    int maxVal = (127 * 127) - 1;
                    
                    // Map the received value to a 16-Bit equivalent
                    val = map(val, 0, maxVal, 0, 65535);
                    
                    Serial.print(String(val) + ", ");
                    
                    channels.channel[i].target[j] = val;
                    // Serial.println(channels.channel[i].target[j]);
                    
                }
                
                // Set interpolation time. Multiply by 100 to get from 0.1s increments to 1ms increments
                // channels.interpolationTime = ((args[14] << 14) + (args[15] << 7) + args[16]) * 100;
                channels.interpolationTime = (args[14] - 1) * (127 * 127);
                channels.interpolationTime += ((args[15] - 1) * 127);
                channels.interpolationTime += args[16] - 1;
                channels.interpolationTime = channels.interpolationTime * 100;
            
            } else {
                
                channels.channel[i].turnOff = true;
                channels.channel[i].turnOn = false;
                
            }
            
        }
        
        Serial.println("Interpolation time: " + String(channels.interpolationTime));
        
        channels.targetValueReached = false;
        channels.startTime = millis();
        // interpolateColors();
        
        return true;
    
    } else {
        
        return false;
        
    }
    
}

bool parseTimer(String args) {
    
    return true;
    
}

void interpolateColors() {
    
    if (millis() < channels.startTime + channels.interpolationTime) {
        
        double t = millis() - channels.startTime;
        // Serial.println(t);
        double perc = t / (double)channels.interpolationTime;
        
        // Serial.println(perc);
        
        for (byte i = 0; i < channels.channelCount; i++) {
            // Serial.println("Writing out channel " + String(i));
            
            for (byte j = 0; j < 3; j++) {
                
                double factor = 0.5 * cos((3.1415 * perc) + 3.1415) + 0.5;
                // Serial.println(factor);
                int val = channels.channel[i].value[j] + (factor * (channels.channel[i].target[j] - channels.channel[i].value[j]));
                photonPWM.analogWrite16GC(channels.channel[i].pin[j], val);
                channels.channel[i].currentValue[j] = val;
                
                // Serial.println(val);
                
            }
            
        }
        
    } else {
        
        channels.targetValueReached = true;
        
        for (byte i = 0; i < channels.channelCount; i++) {
        // Serial.println("Writing out channel " + String(i));
        
        for (byte j = 0; j < 3; j++) {
                
                channels.channel[i].value[j] = channels.channel[i].target[j];
                
            }
            
        }
        
    }
    
}




































