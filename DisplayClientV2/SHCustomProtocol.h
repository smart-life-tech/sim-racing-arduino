#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>
#include <VolvoDIM.h>
#include <EEPROM.h>
#include <mcp2515_can.h>
#include <mcp_can.h>
#include <SPI.h>

VolvoDIM VolvoDIM(9, 6);

// External CAN object from VolvoDIM library
extern mcp2515_can CAN;

class SHCustomProtocol
{
private:
  bool engineOn = false;

  // Blinker state variables
  bool leftBlinkerActive = false;
  bool rightBlinkerActive = false;
  bool leftBlinkerCurrentState = false;
  bool rightBlinkerCurrentState = false;
// BLINKER PATTERN CONTROL
  enum BlinkerPattern {
    PATTERN_LEFT = 0,
    PATTERN_RIGHT = 1,
    PATTERN_BOTH = 2
  };
  
  BlinkerPattern currentPattern = PATTERN_LEFT;
  int blinksInCurrentPattern = 0;
  const int blinksPerPattern = 3; // 3 blinks per pattern
  unsigned long lastPatternChange = 0;
  bool patternJustChanged = false;
  // Timing for blinking
  unsigned long previousBlinkMillis = 0;
  const unsigned long blinkInterval = 1500; // 500ms blink interval

  // SIMULATION MODE VARIABLES
  bool simulationMode = true;
  unsigned long lastSimulationUpdate = 0;
  const unsigned long simulationInterval = 200;
  
  // Simulation data variables
  int simWaterTemp = 180;
  int simCarSpeed = 60;
  int simRpms = 3000;
  int simFuelPercent = 75;
  int simOilTemp = 200;
  String simGear = "3";
  int simBrake = 0;
  int simLeftTurn = 0;
  int simRightTurn = 0;
  bool simIncreasing = true;
  unsigned long simStartTime = 0;
  bool simInitialized = false;

  // Turn signal timing
  unsigned long lastTurnSignalChange = 0;
  const unsigned long turnSignalDuration = 3000; // 3 seconds on, 1 second off
  const unsigned long turnSignalOffDuration = 1000;
  int currentTurnState = 0; // 0=off, 1=left, 2=right, 3=both

  // Odometer variables
  unsigned long lastOdometerValue = 0;
  unsigned long storedOdometerValue = 0;
  bool odometerEnabled = false;

  // EEPROM addresses
  const int EEPROM_ODOMETER_ADDR = 0;
  const int EEPROM_MAGIC_ADDR = 4;
  const unsigned long EEPROM_MAGIC = 0xDEADBEEF;

  // SIMULATION DATA GENERATOR
  void generateSimulationData() {
    if (!simulationMode) return;
    
    unsigned long currentMillis = millis();
    
    // Initialize simulation data once
    if (!simInitialized) {
      simInitialized = true;
      simStartTime = currentMillis;
      lastTurnSignalChange = currentMillis;
      
      // Set initial values
      simRpms = 3000;
      simCarSpeed = 60;
      simWaterTemp = 180;
      simOilTemp = 200;
      simFuelPercent = 75;
      simGear = "3";
      
      Serial.println("Simulation initialized");
      
      // Set initial display values
      updateDisplayValues();
      return;
    }
    
    if (currentMillis - lastSimulationUpdate >= simulationInterval) {
      lastSimulationUpdate = currentMillis;
      
      // Simple RPM cycling
      if (simIncreasing) {
        simRpms += 100;
        if (simRpms >= 6000) {
          simIncreasing = false;
        }
      } else {
        simRpms -= 100;
        if (simRpms <= 1000) {
          simIncreasing = true;
        }
      }
      
      // Update other values
      simCarSpeed = simRpms / 50;
      
      if (simRpms < 1500) simGear = "1";
      else if (simRpms < 2500) simGear = "2";
      else if (simRpms < 3500) simGear = "3";
      else if (simRpms < 4500) simGear = "4";
      else simGear = "5";
      
      simWaterTemp = 170 + (simRpms / 200);
      simOilTemp = 190 + (simRpms / 150);
      
      // Handle turn signal cycling
      handleTurnSignalCycling(currentMillis);
      
      // Update display
      updateDisplayValues();
      
      // Debug output (less frequent)
      static int debugCounter = 0;
      debugCounter++;
      if (debugCounter >= 25) {
        Serial.print("RPM: ");
        Serial.print(simRpms);
        Serial.print(", Turn State: ");
        Serial.print(currentTurnState);
        Serial.print(" (L:");
        Serial.print(leftBlinkerActive ? "ON" : "OFF");
        Serial.print(", R:");
        Serial.print(rightBlinkerActive ? "ON" : "OFF");
        Serial.println(")");
        debugCounter = 0;
      }
    }
  }
  
  void handleTurnSignalCycling(unsigned long currentMilli) {
      // BLINKER PATTERN HANDLER - FIXED VERSION
    unsigned long currentMillis = millis();
    
    // Handle the blinking timing (500ms on/off)
    if (currentMillis - previousBlinkMillis >= blinkInterval) {
      previousBlinkMillis = currentMillis;
      leftBlinkerCurrentState = !leftBlinkerCurrentState;
      rightBlinkerCurrentState = !rightBlinkerCurrentState;
    }
    VolvoDIM.setLeftBlinkerSolid(leftBlinkerCurrentState ? 1 : 0);
    VolvoDIM.simulate();
    VolvoDIM.setRightBlinkerSolid(rightBlinkerCurrentState ? 0 : 0);
    VolvoDIM.simulate();
  }
  
  void updateDisplayValues() {
    // Set time
    int timeValue = VolvoDIM.clockToDecimal(12, 30, 0);
    VolvoDIM.setTime(timeValue);
    
    // Update all gauges
    VolvoDIM.setRpm(simRpms);
    VolvoDIM.setSpeed(simCarSpeed);
    VolvoDIM.setCoolantTemp(simWaterTemp);
    VolvoDIM.setOutdoorTemp(simOilTemp);
    VolvoDIM.setGasLevel(simFuelPercent);
    VolvoDIM.setGearPosText(simGear.charAt(0));
    
    // Handle warning lights based on values
    VolvoDIM.engineServiceRequiredOrange(simRpms > 5500 ? 1 : 0);
    VolvoDIM.fuelFillerCapLoose(simFuelPercent < 20 ? 1 : 0);
    VolvoDIM.engineSystemServiceUrgentRed(simWaterTemp > 210 ? 1 : 0);
    VolvoDIM.slowDownOrShiftUpOrange(simRpms > 5000 ? 1 : 0);
    VolvoDIM.enableTrailer(1); // Always on for testing
    
    // Always set brightness to maximum
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);
  }

  // Function to save odometer to EEPROM
  void saveOdometerToEEPROM(unsigned long mileage) {
    EEPROM.put(EEPROM_ODOMETER_ADDR, mileage);
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  }

  // Function to load odometer from EEPROM
  unsigned long loadOdometerFromEEPROM() {
    unsigned long magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic == EEPROM_MAGIC) {
      unsigned long savedMileage;
      EEPROM.get(EEPROM_ODOMETER_ADDR, savedMileage);
      return savedMileage;
    }
    return 0;
  }

  // Function to parse date/time string
  void parseDateTime(String dateTimeStr, int &hour, int &minute, int &ampm) {
    int spaceIndex = dateTimeStr.indexOf(' ');
    if (spaceIndex == -1) return;

    String timeStr = dateTimeStr.substring(spaceIndex + 1);
    ampm = 0;
    if (timeStr.indexOf("PM") != -1) {
      ampm = 1;
    }

    int firstColon = timeStr.indexOf(':');
    int secondColon = timeStr.indexOf(':', firstColon + 1);

    if (firstColon != -1 && secondColon != -1) {
      hour = timeStr.substring(0, firstColon).toInt();
      minute = timeStr.substring(firstColon + 1, secondColon).toInt();

      if (hour == 0) {
        hour = 12;
      } else if (hour > 12) {
        hour = hour - 12;
      }
    }
  }

  // SIMPLIFIED blinker update - no consecutive count logic for simulation
  void updateBlinkerStates(int leftSignal, int rightSignal) {
    // For simulation mode, we handle blinkers differently
    if (simulationMode) {
      // Don't use this function in simulation mode
      // Blinker states are handled directly in handleTurnSignalCycling
      return;
    }
    
    // Original logic for real SimHub data (when not in simulation)
    static int lastLeftSignal = -1;
    static int lastRightSignal = -1;
    static int leftConsecutiveCount = 0;
    static int rightConsecutiveCount = 0;
    
    // Handle left blinker
    if (leftSignal == lastLeftSignal) {
      leftConsecutiveCount++;
    } else {
      leftConsecutiveCount = 1;
      lastLeftSignal = leftSignal;

      if (leftSignal == 1) {
        leftBlinkerActive = true;
        leftBlinkerCurrentState = true;
        previousBlinkMillis = millis();
      } else if (leftSignal == 0) {
        leftBlinkerActive = false;
        leftBlinkerCurrentState = false;
      }
    }

    if (leftConsecutiveCount >= 5) {
      leftBlinkerActive = false;
      leftBlinkerCurrentState = false;
      leftConsecutiveCount = 0;
    }

    // Handle right blinker
    if (rightSignal == lastRightSignal) {
      rightConsecutiveCount++;
    } else {
      rightConsecutiveCount = 1;
      lastRightSignal = rightSignal;

      if (rightSignal == 1) {
        rightBlinkerActive = true;
        rightBlinkerCurrentState = true;
        previousBlinkMillis = millis();
      } else if (rightSignal == 0) {
        rightBlinkerActive = false;
        rightBlinkerCurrentState = false;
      }
    }

    if (rightConsecutiveCount >= 5) {
      rightBlinkerActive = false;
      rightBlinkerCurrentState = false;
      rightConsecutiveCount = 0;
    }
  }

  // Send custom CAN message
  void sendCustomCANMessage(unsigned long canId, unsigned char *data, int dataLength = 8) {
    CAN.sendMsgBuf(canId, 1, dataLength, data);
  }

  // Enable/disable odometer display
  void enableOdometer(bool enable) {
    odometerEnabled = enable;
    if (enable) {
      storedOdometerValue = loadOdometerFromEEPROM();
      lastOdometerValue = storedOdometerValue;
    }
  }

public:
  // Setup method
  void setup() {
    Serial.begin(115200);
    Serial.println("=== Starting SHCustomProtocol ===");
    
    // Initialize VolvoDIM with delays
    Serial.println("Initializing VolvoDIM...");
    VolvoDIM.gaugeReset();
    delay(500);
    VolvoDIM.init();
    delay(500);
    VolvoDIM.enableSerialErrorMessages();
    delay(100);
    
    // Enable odometer
    enableOdometer(true);
    
    Serial.println("=== Setup Complete - Simulation Active ===");
    // Initialize blinker pattern
    currentPattern = PATTERN_LEFT;
    blinksInCurrentPattern = 0;
    patternJustChanged = true;
    Serial.println("Boot test complete - Starting simulation");
  }

  // Read method (for real data - disabled in simulation)
    // Read method (for real data - disabled in simulation)
  void read() {
    if (simulationMode) return; // Skip if in simulation mode
    
    // Original read implementation for real SimHub data
    int waterTemp = floor(FlowSerialReadStringUntil(',').toInt() * .72);
    int carSpeed = FlowSerialReadStringUntil(',').toInt();
    int rpms = FlowSerialReadStringUntil(',').toInt();
    int fuelPercent = FlowSerialReadStringUntil(',').toInt();
    int oilTemp = FlowSerialReadStringUntil(',').toInt();
    String gear = FlowSerialReadStringUntil(',');
    String currentDateTime = FlowSerialReadStringUntil(',');
    int sessionOdo = FlowSerialReadStringUntil(',').toInt();
    int gameVolume = FlowSerialReadStringUntil(',').toInt();
    int rpmShiftLight = FlowSerialReadStringUntil(',').toInt();
    int brake = FlowSerialReadStringUntil(',').toInt();
    int opponentsCount = FlowSerialReadStringUntil(',').toInt();
    int rightTurn = FlowSerialReadStringUntil(',').toInt();
    int leftTurn = FlowSerialReadStringUntil('\n').toInt();

    // Update blinker states based on incoming signals
    updateBlinkerStates(leftTurn, rightTurn);

    // Parse date/time and set clock
    int hour = 12, minute = 0, ampm = 0;
    parseDateTime(currentDateTime, hour, minute, ampm);
    int timeValue = VolvoDIM.clockToDecimal(hour, minute, ampm);
    VolvoDIM.setTime(timeValue);

    // Update VolvoDIM gauges
    VolvoDIM.setOutdoorTemp(oilTemp);
    VolvoDIM.setCoolantTemp(waterTemp);
    VolvoDIM.setSpeed(carSpeed);
    VolvoDIM.setGasLevel(fuelPercent);
    VolvoDIM.setRpm(rpms);
    VolvoDIM.setGearPosText(gear.charAt(0));

    // Handle all warning lights based on telemetry
    VolvoDIM.engineServiceRequiredOrange(rpms > 7000 ? 1 : 0);
    VolvoDIM.reducedBrakePerformanceOrange(brake > 80 ? 1 : 0);
    VolvoDIM.fuelFillerCapLoose(fuelPercent < 10 ? 1 : 0);
    VolvoDIM.engineSystemServiceUrgentRed(waterTemp > 220 ? 1 : 0);
    VolvoDIM.brakePerformanceReducedRed(brake > 95 ? 1 : 0);
    VolvoDIM.reducedEnginePerformanceRed(oilTemp > 250 ? 1 : 0);
    VolvoDIM.slowDownOrShiftUpOrange(rpmShiftLight > 6500 ? 1 : 0);
    VolvoDIM.reducedEnginePerformanceOrange(oilTemp > 220 ? 1 : 0);
    VolvoDIM.enableTrailer(opponentsCount > 0 ? 1 : 0);

    VolvoDIM.enableDisableDingNoise(gameVolume > 0 ? 0 : 0);

    // Set brightness
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);
  }

  // Main loop method
  void loop() {
    // ALWAYS call VolvoDIM.simulate() first
    VolvoDIM.simulate();
    
    // Generate simulation data if enabled
    if (simulationMode) {
      generateSimulationData();
    }

    // Handle blinker timing - this creates the actual blinking effect
    // unsigned long currentMillis = millis();
    // if (currentMillis - previousBlinkMillis >= blinkInterval) {
    //   previousBlinkMillis = currentMillis;

    //   // Toggle blinker LED states if the blinker is active
    //   if (leftBlinkerActive) {
    //     leftBlinkerCurrentState = !leftBlinkerCurrentState;
    //   } else {
    //     leftBlinkerCurrentState = false;
    //   }

    //   if (rightBlinkerActive) {
    //     rightBlinkerCurrentState = !rightBlinkerCurrentState;
    //   } else {
    //     rightBlinkerCurrentState = false;
    //   }
    // }

    // // Always update hardware state with current LED states
    // VolvoDIM.setLeftBlinkerSolid(leftBlinkerCurrentState ? 1 : 0);
    // VolvoDIM.setRightBlinkerSolid(rightBlinkerCurrentState ? 1 : 0);
    
    // Call simulate again to ensure updates are processed
    VolvoDIM.simulate();
  }

  // Control methods for simulation
  void setSimulationMode(bool enable) {
    simulationMode = enable;
    simInitialized = false;
    if (enable) {
      simStartTime = millis();
      Serial.println("Simulation mode enabled");
    } else {
      Serial.println("Simulation mode disabled");
    }
  }
  
  bool isSimulationMode() {
    return simulationMode;
  }
  
  // Method to manually test blinkers
  void testBlinkers() {
    Serial.println("=== Testing Blinkers ===");
    
    // Test left blinker only
    Serial.println("Testing LEFT blinker for 5 seconds...");
    leftBlinkerActive = true;
    rightBlinkerActive = false;
    leftBlinkerCurrentState = true;
    rightBlinkerCurrentState = false;
    
    unsigned long testStart = millis();
    while (millis() - testStart < 5000) {
      loop(); // This will handle the blinking
      delay(10);
    }
    
    // Test right blinker only
    Serial.println("Testing RIGHT blinker for 5 seconds...");
    leftBlinkerActive = false;
    rightBlinkerActive = true;
    leftBlinkerCurrentState = false;
    rightBlinkerCurrentState = true;
    
    testStart = millis();
    while (millis() - testStart < 5000) {
      loop(); // This will handle the blinking
      delay(10);
    }
    
    // Test both blinkers
    Serial.println("Testing BOTH blinkers for 5 seconds...");
    leftBlinkerActive = true;
    rightBlinkerActive = true;
    leftBlinkerCurrentState = true;
    rightBlinkerCurrentState = true;
    
    testStart = millis();
    while (millis() - testStart < 5000) {
      loop(); // This will handle the blinking
      delay(10);
    }
    
    // Turn off both
    Serial.println("Turning off blinkers...");
    leftBlinkerActive = false;
    rightBlinkerActive = false;
    leftBlinkerCurrentState = false;
    rightBlinkerCurrentState = false;
    VolvoDIM.setLeftBlinkerSolid(0);
    VolvoDIM.setRightBlinkerSolid(0);
    VolvoDIM.simulate();
    
    Serial.println("=== Blinker Test Complete ===");
  }
  
  // Method to set test values
  void setTestValues() {
    Serial.println("Setting test values...");
    
    VolvoDIM.setRpm(4000);
    VolvoDIM.setSpeed(80);
    VolvoDIM.setCoolantTemp(190);
    VolvoDIM.setOutdoorTemp(210);
    VolvoDIM.setGasLevel(50);
    VolvoDIM.setGearPosText('4');
    
    // Turn on some warning lights
    VolvoDIM.engineServiceRequiredOrange(1);
    VolvoDIM.fuelFillerCapLoose(1);
    VolvoDIM.enableTrailer(1);
    
    // Set time
    VolvoDIM.setTime(VolvoDIM.clockToDecimal(2, 45, 1));
    
    // Set brightness
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);
    
    VolvoDIM.simulate();
    Serial.println("Test values set");
  }

  void idle() {
    VolvoDIM.simulate();
  }
};

#endif

