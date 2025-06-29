#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>
#include <VolvoDIM.h>
#include <mcp2515_can.h>
#include <mcp_can.h>
#include <SPI.h>

VolvoDIM VolvoDIM(9, 6);

// External CAN object from VolvoDIM library
extern mcp2515_can CAN;

class SHCustomProtocol
{
  private:
    // Pack boolean variables into bit fields to save memory
    struct {
      bool leftActive : 1;
      bool rightActive : 1;
      bool tcActive : 1;
      bool absActive : 1;
      bool highBeamActive : 1;
      bool tcState : 1;
      bool absState : 1;
      bool autoTestEnabled : 1;
    } flags;
    
    // Use smaller data types
    uint8_t testPhase;
    uint16_t blinkerUpdateInterval;
    uint16_t warningUpdateInterval;
    uint16_t phaseDuration;
    
    // Timing variables
    unsigned long lastBlinkerUpdate;
    unsigned long lastWarningUpdate;
    unsigned long phaseStart;
    
    // Custom CAN functions - simplified
    void sendCANMessage(uint8_t canId, bool state) {
      uint8_t data[8] = {0};
      data[0] = state ? 0x01 : 0x00;
      CAN.sendMsgBuf(canId, 1, 8, data);
    }
    
    void updateBlinkers() {
      unsigned long now = millis();
      
      if (now - lastBlinkerUpdate >= blinkerUpdateInterval) {
        lastBlinkerUpdate = now;
        
        // Use VolvoDIM's built-in blinker system
        VolvoDIM.setLeftBlinker(flags.leftActive ? 1 : 0);
        VolvoDIM.setRightBlinker(flags.rightActive ? 1 : 0);
      }
    }

    void handleWarningLights() {
      unsigned long now = millis();
      
      if (now - lastWarningUpdate >= warningUpdateInterval) {
        lastWarningUpdate = now;
        
        // Toggle TC and ABS if active
        if (flags.tcActive) {
          flags.tcState = !flags.tcState;
        } else {
          flags.tcState = false;
        }
        
        if (flags.absActive) {
          flags.absState = !flags.absState;
        } else {
          flags.absState = false;
        }
        
        // Send CAN messages
        sendCANMessage(4, flags.tcState);    // TC
        sendCANMessage(5, flags.absState);   // ABS
        sendCANMessage(6, flags.highBeamActive); // High Beam
      }
    }

    void runTestSequence() {
      if (!flags.autoTestEnabled) return;
      
      unsigned long now = millis();
      
      if (now - phaseStart >= phaseDuration) {
        phaseStart = now;
        testPhase = (testPhase + 1) % 8;
        
        // Reset all states
        flags.leftActive = flags.rightActive = false;
        flags.tcActive = flags.absActive = flags.highBeamActive = false;
        
        switch (testPhase) {
          case 0: flags.leftActive = true; break;
          case 1: flags.rightActive = true; break;
          case 2: flags.leftActive = flags.rightActive = true; break;
          case 3: flags.tcActive = true; break;
          case 4: flags.absActive = true; break;
          case 5: flags.highBeamActive = true; break;
          case 6: 
            flags.leftActive = flags.rightActive = true;
            flags.tcActive = flags.absActive = flags.highBeamActive = true;
            break;
          case 7: break; // All off
        }
      }
    }

    void handleSerialCommands() {
      if (Serial.available()) {
        char cmd = Serial.read();
        
        // Simple single character commands to save memory
        switch (cmd) {
          case '1': flags.leftActive = true; flags.autoTestEnabled = false; break;
          case '2': flags.leftActive = false; break;
          case '3': flags.rightActive = true; flags.autoTestEnabled = false; break;
          case '4': flags.rightActive = false; break;
          case '5': flags.tcActive = true; flags.autoTestEnabled = false; break;
          case '6': flags.tcActive = false; break;
          case '7': flags.absActive = true; flags.autoTestEnabled = false; break;
          case '8': flags.absActive = false; break;
          case '9': flags.highBeamActive = true; flags.autoTestEnabled = false; break;
          case '0': flags.highBeamActive = false; break;
          case 'a': flags.autoTestEnabled = true; break;
          case 's': flags.autoTestEnabled = false; break;
          case 'h': printHelp(); break;
          case 'x': // All off
            flags.leftActive = flags.rightActive = false;
            flags.tcActive = flags.absActive = flags.highBeamActive = false;
            break;
        }
        
        // Clear remaining characters
        while (Serial.available()) Serial.read();
      }
    }

    void printHelp() {
      // Use F() macro to store strings in flash memory instead of RAM
      Serial.println(F("=== COMMANDS ==="));
      Serial.println(F("1/2 - Left ON/OFF"));
      Serial.println(F("3/4 - Right ON/OFF"));
      Serial.println(F("5/6 - TC ON/OFF"));
      Serial.println(F("7/8 - ABS ON/OFF"));
      Serial.println(F("9/0 - HB ON/OFF"));
      Serial.println(F("a/s - Auto ON/OFF"));
      Serial.println(F("x - All OFF"));
      Serial.println(F("h - Help"));
    }

  public:
    void setup() {
      Serial.begin(115200);
      Serial.println(F("=== Blinker Test ==="));
      
      // Initialize bit field structure
      flags.leftActive = false;
      flags.rightActive = false;
      flags.tcActive = false;
      flags.absActive = false;
      flags.highBeamActive = false;
      flags.tcState = false;
      flags.absState = false;
      flags.autoTestEnabled = true;
      
      // Initialize variables
      testPhase = 0;
      blinkerUpdateInterval = 100;
      warningUpdateInterval = 1000;
      phaseDuration = 3000;
      
      VolvoDIM.gaugeReset();
      delay(1000);
      VolvoDIM.init();
      delay(1000);
      
      // Set basic display values
      VolvoDIM.setTotalBrightness(255);
      VolvoDIM.setRpm(2000);
      VolvoDIM.setSpeed(45);
      VolvoDIM.setCoolantTemp(180);
      VolvoDIM.setGasLevel(75);
      VolvoDIM.setGearPosText('D');
      
      // Initialize timing
      phaseStart = millis();
      lastBlinkerUpdate = millis();
      lastWarningUpdate = millis();
      flags.leftActive = true;
      
      Serial.println(F("Ready. Type 'h' for help"));
    }

    void loop() {
      VolvoDIM.simulate();
      handleSerialCommands();
      runTestSequence();
      updateBlinkers();
      handleWarningLights();
      delay(10);
    }

    // Minimal compatibility methods
    void read() { }
    void idle() { VolvoDIM.simulate(); }
    
    // Simple setters
    void setLeft(bool on) { flags.leftActive = on; flags.autoTestEnabled = false; }
    void setRight(bool on) { flags.rightActive = on; flags.autoTestEnabled = false; }
    void setTC(bool on) { flags.tcActive = on; flags.autoTestEnabled = false; }
    void setABS(bool on) { flags.absActive = on; flags.autoTestEnabled = false; }
    void setHighBeam(bool on) { flags.highBeamActive = on; flags.autoTestEnabled = false; }
};

#endif
