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
    bool leftBlinkerActive = true;
    bool rightBlinkerActive = true;
    bool leftBlinkerCurrentState = false;  // Current LED state (on/off)
    bool rightBlinkerCurrentState = false; // Current LED state (on/off)

    // Consecutive signal counters
    int leftConsecutiveCount = 0;
    int rightConsecutiveCount = 0;
    int lastLeftSignal = -1;
    int lastRightSignal = -1;

    // Timing for blinking
    unsigned long previousBlinkMillis = 0;
    const unsigned long blinkInterval = 600; // 500ms blink interval (faster for testing)

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

      if (magic == EEPROM_MAGIC)
      {
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

        // Start blinking when signal goes to 1
        if (leftSignal == 1)
        {
          leftBlinkerActive = true;
          leftBlinkerCurrentState = true; // Start with LED on
        }
      }

      // Stop left blinker after 5 consecutive identical signals
      if (leftConsecutiveCount >= 5)
      {
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

        // Start blinking when signal goes to 1
        if (rightSignal == 1)
        {
          rightBlinkerActive = true;
          rightBlinkerCurrentState = true; // Start with LED on
        }
      }

      // Stop right blinker after 5 consecutive identical signals
      if (rightConsecutiveCount >= 5)
      {
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

    // Custom odometer display function with EEPROM persistence
    void setOdometer(unsigned long mileage)
    {
      if (!odometerEnabled)
      {
        return;
      }

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
      //VolvoDIM.set4CWarning(true);
      //VolvoDIM.setSRSWarning(true);
      
      if (storedOdometerValue > 0)
      {
        setOdometer(storedOdometerValue);
      }
      VolvoDIM.setLeftBlinker(1);
      VolvoDIM.setRightBlinker(1);
      VolvoDIM.setABSWarning(true);
      VolvoDIM.setTCWarning(true);
      const char* text = "Volvo DIM Custom Protocol";
      VolvoDIM.setCustomText(text);
    }

    // Called when new data is coming from computer - ONLY update states, no blinking logic
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

      // ONLY update blinker states based on incoming signals - NO blinking here
      updateBlinkerStates(leftTurn, rightTurn);

      // Parse date/time and set clock
      int hour = 12, minute = 0, ampm = 0;
      parseDateTime(currentDateTime, hour, minute, ampm);
      int timeValue = VolvoDIM.clockToDecimal(hour, minute, ampm);
      VolvoDIM.setTime(timeValue);
      //rpms = map(rpms, 0, 8000, 0, 9000);
      // Update VolvoDIM gauges
      VolvoDIM.setOutdoorTemp(oilTemp);
      VolvoDIM.setCoolantTemp(waterTemp);
      VolvoDIM.setSpeed(carSpeed);
      VolvoDIM.setGasLevel(fuelPercent);
      VolvoDIM.setRpm(rpms);
      VolvoDIM.setGearPosText(gear.charAt(0));

      // Set persistent odometer display
      // setOdometer(totalOdometer);
      VolvoDIM.enableMilageTracking(1);
      // VolvoDIM.setCustomText((String("Odo: ") + String(totalOdometer) + " km").c_str());
      //  Handle all warning lights based on telemetry
      handleWarningLights(rpms, waterTemp, oilTemp, fuelPercent, brake, carSpeed, opponentsCount, rpmShiftLight);

      VolvoDIM.enableDisableDingNoise(gameVolume > 0 ? 0 : 0);

      // Set brightness based on shift light
      int brightness = map(rpmShiftLight, 0, 8000, 50, 256);
      VolvoDIM.setTotalBrightness(255);
      VolvoDIM.setOverheadBrightness(255);
      VolvoDIM.setLcdBrightness(255);
      VolvoDIM.setRightBlinker(rightTurn ? 1 : 0);
      VolvoDIM.setLeftBlinker(leftTurn ? 1 : 0);
    }

    // Called continuously - HANDLE ALL BLINKING LOGIC HERE
    void loop()
    {
      VolvoDIM.simulate();

      // Handle blinking timing continuously
      unsigned long currentMillis = millis();

      if (currentMillis - previousBlinkMillis >= blinkInterval)
      {
        previousBlinkMillis = currentMillis;

        // Toggle blinker states ONLY if they're active
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
        FlowSerialPrintLn("Blinker states: " + String(leftBlinkerCurrentState) + ", " + String(rightBlinkerCurrentState));
        //VolvoDIM.setLeftBlinkerSolid(leftBlinkerCurrentState ? 1 : 0);
        //VolvoDIM.setRightBlinkerSolid(leftBlinkerCurrentState ? 1 : 0);
        // VolvoDIM.setLeftBlinker(leftBlinkerCurrentState ? 1 : 1);
        // VolvoDIM.setRightBlinker(rightBlinkerCurrentState ? 1 : 1);
        //VolvoDIM.set4CWarning(true);
        // Direction lamp ON (extended ID)
        uint8_t directionLamp[8]  = {0xCE, 0x51, 0xB0, 0x09, 0x01, 0xFF, 0x01, 0x00};
        CAN.sendMsgBuf(0xFFFFE, 1, 8, directionLamp); // 1 = extended ID
        Serial.println("Direction lamp command sent.");

        delay(10);


        // Fog lamp ON (extended ID)
        uint8_t fogLamp[8]  = {0xCE, 0x51, 0xB0, 0x09, 0x01, 0xFF, 0x02, 0x00};
        CAN.sendMsgBuf(0xFFFFE, 1, 8, fogLamp); // 1 = extended ID
        Serial.println("Fog lamp command sent.");

        delay(10);


      // ABS lamp ON (extended ID)
        uint8_t absLamp[8]  = {0xCE, 0x51, 0xB0, 0x09, 0x01, 0xFF, 0x04, 0x00};
        CAN.sendMsgBuf(0xFFFFE, 1, 8, absLamp); // 1 = extended ID
        Serial.println("ABS lamp command sent.");

        delay(10);

        // SPIN lamp ON (extended ID)
        uint8_t spinLamp[8] = {0xCE, 0x51, 0xB0, 0x09, 0x01, 0xFF, 0x08, 0x00};
        CAN.sendMsgBuf(0xFFFFE, 1, 8, spinLamp); // 1 = extended ID
        Serial.println("SPIN lamp command sent.");
        VolvoDIM.setSRSWarning(true,0x1A0600A);
        VolvoDIM.simulate();
      }




    }

    void idle()
    {
    }
};

#endif
