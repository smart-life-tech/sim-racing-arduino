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
  
  // Blinker variables
  int leftTurns = 0;
  int rightTurns = 0;
  unsigned long counter = 0;
  bool blinkerEnabled = false;
  unsigned long previousMillis = 0;
  long interval = 1000; // Check every second
  
  // Odometer variables
  unsigned long lastOdometerValue = 0;
  unsigned long storedOdometerValue = 0;
  bool odometerEnabled = false;
  
  // EEPROM addresses for storing odometer
  const int EEPROM_ODOMETER_ADDR = 0;
  const int EEPROM_MAGIC_ADDR = 4;
  const unsigned long EEPROM_MAGIC = 0xDEADBEEF;

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
    
    if (magic == EEPROM_MAGIC) {
      unsigned long savedMileage;
      EEPROM.get(EEPROM_ODOMETER_ADDR, savedMileage);
      return savedMileage;
    }
    return 0;
  }

  // Function to parse date/time string and extract hour/minute/AM-PM
  void parseDateTime(String dateTimeStr, int &hour, int &minute, int &ampm)
  {
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

  // Updated blinker function that handles persistence
  void enableBlinker(int state1, int state2)
  {
    counter++;
    
    // Set turn signals when active
    if (state1)
      leftTurns = 1;
    if (state2)
      rightTurns = 1;
    
    // Reset counter if either signal is active
    if (state1 || state2)
      counter = 0;
    
    // After 5 cycles of no input, disable blinkers and clear turn signals
    if (counter > 5)
    {
      counter = 0;
      leftTurns = 0;   // Clear left turn signal
      rightTurns = 0;  // Clear right turn signal
      blinkerEnabled = false;
    }
    else
    {
      blinkerEnabled = true;
    }
  }

  // Send custom CAN message
  void sendCustomCANMessage(unsigned long canId, unsigned char* data, int dataLength = 8)
  {
    CAN.sendMsgBuf(canId, 1, dataLength, data);
  }

  // Custom odometer display function with EEPROM persistence
  void setOdometer(unsigned long mileage)
  {
    if (!odometerEnabled) {
      return;
    }
    
    if (mileage == 0 || mileage < storedOdometerValue) {
      mileage = storedOdometerValue;
    }
    
    if (abs((long)(mileage - lastOdometerValue)) > 0) {
      lastOdometerValue = mileage;
      
      if (mileage - storedOdometerValue >= 10 || mileage < storedOdometerValue) {
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
    if (enable) {
      storedOdometerValue = loadOdometerFromEEPROM();
      lastOdometerValue = storedOdometerValue;
    }
  }

  // Handle all warning lights based on telemetry data
  void handleWarningLights(int rpms, int waterTemp, int oilTemp, int fuelPercent, int brake, int carSpeed, int opponentsCount, int rpmShiftLight)
  {
    VolvoDIM.engineServiceRequiredOrange(rpms > 7000 ? 1 : 0);
    VolvoDIM.reducedBrakePerformanceOrange(brake > 80 ? 1 : 0);
    VolvoDIM.fuelFillerCapLoose(fuelPercent < 10 ? 1 : 0);
    VolvoDIM.engineSystemServiceUrgentRed(waterTemp > 220 ? 1 : 0);
    VolvoDIM.brakePerformanceReducedRed(brake > 95 ? 1 : 0);
    VolvoDIM.reducedEnginePerformanceRed(oilTemp > 250 ? 1 : 0);
    VolvoDIM.slowDownOrShiftUpOrange(rpmShiftLight > 6500 ? 1 : 0);
    VolvoDIM.reducedEnginePerformanceOrange(oilTemp > 220 ? 1 : 0);
    VolvoDIM.enableTrailer(opponentsCount > 0 ? 1 : 0);
  }

public:
  // Called when starting the arduino (setup method in main sketch)
  void setup()
  {
    VolvoDIM.gaugeReset();
    VolvoDIM.init();
    VolvoDIM.enableSerialErrorMessages();
    enableOdometer(true);
    
    if (storedOdometerValue > 0) {
      setOdometer(storedOdometerValue);
    }
  }

  // Called when new data is coming from computer
  void read()
  {
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

    // Update blinker persistence
    enableBlinker(leftTurn, rightTurn);

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
    
    // Set persistent odometer display
    setOdometer(totalOdometer);
    
    // Handle all warning lights based on telemetry
    handleWarningLights(rpms, waterTemp, oilTemp, fuelPercent, brake, carSpeed, opponentsCount, rpmShiftLight);
    
    VolvoDIM.enableDisableDingNoise(gameVolume > 0 ? 0 : 0);
    
    // Set brightness based on shift light
    int brightness = map(rpmShiftLight, 0, 8000, 50, 256);
    VolvoDIM.setTotalBrightness(255);
    VolvoDIM.setOverheadBrightness(255);
    VolvoDIM.setLcdBrightness(255);
  }

  // Called once per arduino loop - handle blinker timing here
  void loop()
  {
    VolvoDIM.simulate();
    
    unsigned long currentMillis = millis();
    if ((currentMillis - previousMillis >= interval))
    {
      previousMillis = currentMillis;
      
      // Handle blinker logic based on persistent variables
      if (rightTurns == 1 && leftTurns == 1)
      {
        // Both blinkers on (hazard lights)
        VolvoDIM.setRightBlinker(1);
        VolvoDIM.setLeftBlinker(1);
      }
      else if (rightTurns == 0 && leftTurns == 0)
      {
        // Both blinkers off
        VolvoDIM.setRightBlinker(0);
        VolvoDIM.setLeftBlinker(0);
      }
      else if (rightTurns == 1 && leftTurns == 0)
      {
        // Right blinker only
        VolvoDIM.setRightBlinker(1);
        VolvoDIM.setLeftBlinker(0);
      }
      else if (leftTurns == 1 && rightTurns == 0)
      {
        // Left blinker only
        VolvoDIM.setLeftBlinker(1);
        VolvoDIM.setRightBlinker(0);
      }
    }
  }

  void idle()
  {
  }
};

#endif
