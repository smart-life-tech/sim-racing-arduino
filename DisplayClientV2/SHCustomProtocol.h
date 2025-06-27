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

  // Consecutive signal counters
  int leftConsecutiveCount = 0;
  int rightConsecutiveCount = 0;
  int lastLeftSignal = -1;
  int lastRightSignal = -1;

  // Timing for blinking
  unsigned long previousBlinkMillis = 0;
  const unsigned long blinkInterval = 500;

  // SIMULATION MODE VARIABLES
  bool simulationMode = true; // Enable simulation by default
  unsigned long lastSimulationUpdate = 0;
  const unsigned long simulationInterval = 200; // Slower updates - every 200ms

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

  // Odometer variables
  unsigned long lastOdometerValue = 0;
  unsigned long storedOdometerValue = 0;
  bool odometerEnabled = false;

  // EEPROM addresses
  const int EEPROM_ODOMETER_ADDR = 0;
  const int EEPROM_MAGIC_ADDR = 4;
  const unsigned long EEPROM_MAGIC = 0xDEADBEEF;

  // SIMULATION DATA GENERATOR
  void generateSimulationData()
  {
    if (!simulationMode)
      return;

    unsigned long currentMillis = millis();

    // Initialize simulation data once
    if (!simInitialized)
    {
      simInitialized = true;
      simStartTime = currentMillis;

      // Set initial values that should be visible
      simRpms = 3000;
      simCarSpeed = 60;
      simWaterTemp = 180;
      simOilTemp = 200;
      simFuelPercent = 75;
      simGear = "3";

      Serial.println("Simulation initialized with test values");

      // Set initial display values
      VolvoDIM.setRpm(simRpms);
      VolvoDIM.setSpeed(simCarSpeed);
      VolvoDIM.setCoolantTemp(simWaterTemp);
      VolvoDIM.setOutdoorTemp(simOilTemp);
      VolvoDIM.setGasLevel(simFuelPercent);
      VolvoDIM.setGearPosText(simGear.charAt(0));

      // Turn on some warning lights for testing
      VolvoDIM.engineServiceRequiredOrange(1);
      VolvoDIM.fuelFillerCapLoose(1);
      VolvoDIM.enableTrailer(1);

      // Set brightness to maximum
      VolvoDIM.setTotalBrightness(255);
      VolvoDIM.setOverheadBrightness(255);
      VolvoDIM.setLcdBrightness(255);

      return; // Exit early on first run
    }

    if (currentMillis - lastSimulationUpdate >= simulationInterval)
    {
      lastSimulationUpdate = currentMillis;

      // Simple RPM cycling
      if (simIncreasing)
      {
        simRpms += 100;
        if (simRpms >= 6000)
        {
          simIncreasing = false;
        }
      }
      else
      {
        simRpms -= 100;
        if (simRpms <= 1000)
        {
          simIncreasing = true;
        }
      }

      // Update speed based on RPM
      simCarSpeed = simRpms / 50;

      // Update gear based on RPM
      if (simRpms < 1500)
        simGear = "1";
      else if (simRpms < 2500)
        simGear = "2";
      else if (simRpms < 3500)
        simGear = "3";
      else if (simRpms < 4500)
        simGear = "4";
      else
        simGear = "5";

      // Vary temperatures slightly
      simWaterTemp = 170 + (simRpms / 200);
      simOilTemp = 190 + (simRpms / 150);

      // Simulate turn signals (cycle every 6 seconds)
      int turnCycle = ((currentMillis - simStartTime) / 6000) % 3;
      switch (turnCycle)
      {
      case 0:
        simLeftTurn = 0;
        simRightTurn = 0;
        break;
      case 1:
        simLeftTurn = 1;
        simRightTurn = 0;
        break;
      case 2:
        simLeftTurn = 0;
        simRightTurn = 1;
        break;
      }

      // Process the simulated data
      processSimulatedData();

      // Debug output (less frequent)
      static int debugCounter = 0;
      debugCounter++;
      if (debugCounter >= 25)
      { // Every 5 seconds (25 * 200ms)
        Serial.print("Sim Data - RPM: ");
        Serial.print(simRpms);
        Serial.print(", Speed: ");
        Serial.print(simCarSpeed);
        Serial.print(", Gear: ");
        Serial.println(simGear);
        debugCounter = 0;
      }
    }
  }

  void processSimulatedData()
  {
    // Update blinker states
    updateBlinkerStates(simLeftTurn, simRightTurn);

    // Set time (simulate current time)
    int timeValue = VolvoDIM.clockToDecimal(12, 30, 0); // Fixed time for testing
    VolvoDIM.setTime(timeValue);

    // Update all gauges with current simulation values
    VolvoDIM.setRpm(simRpms);
    VolvoDIM.setSpeed(simCarSpeed);
    VolvoDIM.setCoolantTemp(simWaterTemp);
    VolvoDIM.setOutdoorTemp(simOilTemp);
    VolvoDIM.setGasLevel(simFuelPercent);
    VolvoDIM.setGearPosText(simGear.charAt(0));

    // Handle warning lights based on values
    VolvoDIM.engineServiceRequiredOrange(simRpms > 5500 ? 1 : 0);
    VolvoDIM.reducedBrakePerformanceOrange(simBrake > 80 ? 1 : 0);
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
  void saveOdometerToEEPROM(unsigned long mileage)
  {
    EEPROM.put(EEPROM_ODOMETER_ADDR, mileage);
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  }

  // Function to load odometer from EEPROM
  unsigned long loadOdometerFromEEPROM()
  {
    unsigned long magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic == EEPROM_MAGIC)
    {
      unsigned long savedMileage;
      EEPROM.get(EEPROM_ODOMETER_ADDR, savedMileage);
      return savedMileage;
    }
    return 0;
  }

  // Function to parse date/time string
  void parseDateTime(String dateTimeStr, int &hour, int &minute, int &ampm)
  {
    int spaceIndex = dateTimeStr.indexOf(' ');
    if (spaceIndex == -1)
      return;

    String timeStr = dateTimeStr.substring(spaceIndex + 1);
    ampm = 0;
    if (timeStr.indexOf("PM") != -1)
    {
      ampm = 1;
    }

    int firstColon = timeStr.indexOf(':');
    int secondColon = timeStr.indexOf(':', firstColon + 1);

    if (firstColon != -1 && secondColon != -1)
    {
      hour = timeStr.substring(0, firstColon).toInt();
      minute = timeStr.substring(firstColon + 1, secondColon).toInt();

      if (hour == 0)
      {
        hour = 12;
      }
      else if (hour > 12)
      {
        hour = hour - 12;
      }
    }
  }

  // Handle blinker signals and update active states
  void updateBlinkerStates(int leftSignal, int rightSignal)
  {
    // Handle left blinker
    if (leftSignal == lastLeftSignal)
    {
      leftConsecutiveCount++;
    }
    else
    {
      leftConsecutiveCount = 1;
      lastLeftSignal = leftSignal;

      if (leftSignal == 1)
      {
        leftBlinkerActive = true;
        leftBlinkerCurrentState = true;
        previousBlinkMillis = millis();
      }
      else if (leftSignal == 0)
      {
        leftBlinkerActive = false;
        leftBlinkerCurrentState = false;
      }
    }

    if (leftConsecutiveCount >= 10)
    { // Increased from 5 to 10
      leftBlinkerActive = false;
      leftBlinkerCurrentState = false;
      leftConsecutiveCount = 0;
    }

    // Handle right blinker
    if (rightSignal == lastRightSignal)
    {
      rightConsecutiveCount++;
    }
    else
    {
      rightConsecutiveCount = 1;
      lastRightSignal = rightSignal;

      if (rightSignal == 1)
      {
        rightBlinkerActive = true;
        rightBlinkerCurrentState = true;
        previousBlinkMillis = millis();
      }
      else if (rightSignal == 0)
      {
        rightBlinkerActive = false;
        rightBlinkerCurrentState = false;
      }
    }

    if (rightConsecutiveCount >= 10)
    { // Increased from 5 to 10
      rightBlinkerActive = false;
      rightBlinkerCurrentState = false;
      rightConsecutiveCount = 0;
    }
  }

  // Send custom CAN message
  void sendCustomCANMessage(unsigned long canId, unsigned char *data, int dataLength = 8)
  {
    CAN.sendMsgBuf(canId, 1, dataLength, data);
  }

  // Custom odometer display function
  void setOdometer(unsigned long mileage)
  {
    if (!odometerEnabled)
      return;

    if (mileage == 0 || mileage < storedOdometerValue)
    {
      mileage = storedOdometerValue;
    }

    if (abs((long)(mileage - lastOdometerValue)) > 0)
    {
      lastOdometerValue = mileage;

      if (mileage - storedOdometerValue >= 10 || mileage < storedOdometerValue)
      {
        storedOdometerValue = mileage;
        saveOdometerToEEPROM(mileage);
      }

      unsigned long odometerCanId = 0x217FFC;
      unsigned char odometerData[8] = {0x01, 0xEB, 0x00, 0xD8, 0xF0, 0x58, 0x00, 0x00};
      odometerData[7] = (mileage & 0xFF);
      sendCustomCANMessage(odometerCanId, odometerData);
      delay(10);
    }
  }

  // Enable/disable odometer display
  void enableOdometer(bool enable)
  {
    odometerEnabled = enable;
    if (enable)
    {
      storedOdometerValue = loadOdometerFromEEPROM();
      lastOdometerValue = storedOdometerValue;
    }
  }

public:
  // Setup method
  void setup()
  {
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

    // Shorter boot test - just flash once
    VolvoDIM.setLeftBlinkerSolid(1);
    VolvoDIM.setRightBlinkerSolid(1);
    VolvoDIM.simulate();
    delay(500);
    VolvoDIM.setLeftBlinkerSolid(0);
    VolvoDIM.setRightBlinkerSolid(0);
    VolvoDIM.simulate();

    Serial.println("Boot test complete - Starting simulation");
  }

  // Read method (for real data - disabled in simulation)
  void read()
  {
    if (simulationMode)
      return; // Skip if in simulation mode

    // Original read implementation would go here
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
    unsigned long totalOdometer = sessionOdo;

    // ONLY update blinker states based on incoming signals - NO blinking here
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

    // Set brightness based on shift light
    int brightness = map(rpmShiftLight, 0, 8000, 50, 256);
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);
  }

  // Main loop method
  void loop()
  {
    // ALWAYS call VolvoDIM.simulate() first and frequently
    VolvoDIM.simulate();

    // Generate simulation data if enabled
    if (simulationMode)
    {
      generateSimulationData();
    }

    // Handle blinker timing
    unsigned long currentMillis = millis();
    if (currentMillis - previousBlinkMillis >= blinkInterval)
    {
      previousBlinkMillis = currentMillis;

      // Toggle blinker states if active
      if (leftBlinkerActive)
      {
        leftBlinkerCurrentState = !leftBlinkerCurrentState;
      }
      else
      {
        leftBlinkerCurrentState = false;
      }

      if (rightBlinkerActive)
      {
        rightBlinkerCurrentState = !rightBlinkerCurrentState;
      }
      else
      {
        rightBlinkerCurrentState = false;
      }
    }

    // Always update hardware state
    VolvoDIM.setLeftBlinkerSolid(leftBlinkerCurrentState ? 1 : 0);
    VolvoDIM.setRightBlinkerSolid(rightBlinkerCurrentState ? 1 : 0);

    // Call simulate again to ensure updates are processed
    VolvoDIM.simulate();
  }

  // Control methods for simulation
  void setSimulationMode(bool enable)
  {
    simulationMode = enable;
    simInitialized = false; // Reset initialization flag
    if (enable)
    {
      simStartTime = millis();
      Serial.println("Simulation mode enabled");
    }
    else
    {
      Serial.println("Simulation mode disabled");
    }
  }

  bool isSimulationMode()
  {
    return simulationMode;
  }

  // Method to manually set test values
  void setTestValues()
  {
    Serial.println("Setting test values...");

    // Set fixed test values
    VolvoDIM.setRpm(4000);
    VolvoDIM.setSpeed(80);
    VolvoDIM.setCoolantTemp(190);
    VolvoDIM.setOutdoorTemp(210);
    VolvoDIM.setGasLevel(50);
    VolvoDIM.setGearPosText('4');

    // Turn on warning lights
    VolvoDIM.engineServiceRequiredOrange(1);
    VolvoDIM.fuelFillerCapLoose(1);
    VolvoDIM.enableTrailer(1);
    VolvoDIM.slowDownOrShiftUpOrange(1);

    // Set time
    VolvoDIM.setTime(VolvoDIM.clockToDecimal(2, 45, 1)); // 2:45 PM

    // Set brightness
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);

    // Force update
    VolvoDIM.simulate();

    Serial.println("Test values set");
  }

  // Method to test all indicators individually
  void testAllIndicators()
  {
    Serial.println("=== Testing All Indicators ===");

    // Test each warning light individually
    Serial.println("Testing Engine Service Orange...");
    VolvoDIM.engineServiceRequiredOrange(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.engineServiceRequiredOrange(0);

    Serial.println("Testing Brake Performance Orange...");
    VolvoDIM.reducedBrakePerformanceOrange(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.reducedBrakePerformanceOrange(0);

    Serial.println("Testing Fuel Cap Loose...");
    VolvoDIM.fuelFillerCapLoose(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.fuelFillerCapLoose(0);

    Serial.println("Testing Engine System Red...");
    VolvoDIM.engineSystemServiceUrgentRed(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.engineSystemServiceUrgentRed(0);

    Serial.println("Testing Brake Performance Red...");
    VolvoDIM.brakePerformanceReducedRed(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.brakePerformanceReducedRed(0);

    Serial.println("Testing Engine Performance Red...");
    VolvoDIM.reducedEnginePerformanceRed(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.reducedEnginePerformanceRed(0);

    Serial.println("Testing Shift Up Orange...");
    VolvoDIM.slowDownOrShiftUpOrange(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.slowDownOrShiftUpOrange(0);

    Serial.println("Testing Engine Performance Orange...");
    VolvoDIM.reducedEnginePerformanceOrange(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.reducedEnginePerformanceOrange(0);

    Serial.println("Testing Trailer...");
    VolvoDIM.enableTrailer(1);
    VolvoDIM.simulate();
    delay(1000);
    VolvoDIM.enableTrailer(0);

    // Test gauges
    Serial.println("Testing Gauges...");
    for (int i = 0; i <= 100; i += 20)
    {
      VolvoDIM.setRpm(i * 60);              // 0 to 6000 RPM
      VolvoDIM.setSpeed(i);                 // 0 to 100 speed
      VolvoDIM.setGasLevel(100 - i);        // 100 to 0 fuel
      VolvoDIM.setCoolantTemp(160 + i / 5); // 160 to 180 temp
      VolvoDIM.setOutdoorTemp(180 + i / 5); // 180 to 200 temp
      VolvoDIM.simulate();
      delay(500);
    }

    // Test blinkers
    Serial.println("Testing Blinkers...");
    for (int i = 0; i < 6; i++)
    {
      VolvoDIM.setLeftBlinkerSolid(1);
      VolvoDIM.setRightBlinkerSolid(1);
      VolvoDIM.simulate();
      delay(300);
      VolvoDIM.setLeftBlinkerSolid(0);
      VolvoDIM.setRightBlinkerSolid(0);
      VolvoDIM.simulate();
      delay(300);
    }

    Serial.println("=== Test Complete ===");
  }

  void idle()
  {
    // Called when idle - keep VolvoDIM active
    VolvoDIM.simulate();
  }
};

#endif
// End of SHCustomProtocol.h
