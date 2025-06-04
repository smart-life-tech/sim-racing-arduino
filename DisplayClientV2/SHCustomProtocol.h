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
  bool engineOn = false; // Variable to track engine state
  bool leftBlinkerState = false;
  bool rightBlinkerState = false;
  unsigned long lastBlinkMillisLeft = 0;
  unsigned long lastBlinkMillisRight = 0;
  unsigned long previousMillisLeft = 0;
  unsigned long previousMillisRight = 0;
  const unsigned long blinkInterval = 500; // Blink interval in milliseconds
  int previousLeftTurn = 0;
  int previousRightTurn = 0;

  // Odometer variables
  unsigned long lastOdometerValue = 0;
  bool odometerEnabled = false;

  // Function to parse date/time string and extract hour/minute/AM-PM
  void parseDateTime(String dateTimeStr, int &hour, int &minute, int &ampm)
  {
    // Format: "3/6/2025 05:45:34 PM"
    // Find the space before the time
    int spaceIndex = dateTimeStr.indexOf(' ');
    if (spaceIndex == -1)
      return;

    // Extract time portion: "05:45:34 PM"
    String timeStr = dateTimeStr.substring(spaceIndex + 1);

    // Find AM/PM
    ampm = 0; // Default to AM
    if (timeStr.indexOf("PM") != -1)
    {
      ampm = 1;
    }

    // Extract hour and minute
    int firstColon = timeStr.indexOf(':');
    int secondColon = timeStr.indexOf(':', firstColon + 1);

    if (firstColon != -1 && secondColon != -1)
    {
      hour = timeStr.substring(0, firstColon).toInt();
      minute = timeStr.substring(firstColon + 1, secondColon).toInt();

      // Convert 24-hour to 12-hour format if needed
      if (hour == 0)
      {
        hour = 12; // Midnight becomes 12 AM
      }
      else if (hour > 12)
      {
        hour = hour - 12; // Convert PM hours
      }
    }
  }

  // Handle blinker logic with timing
  void handleBlinker(int currentTurn, bool &blinkerState, unsigned long &lastBlinkMillis, int &previousTurn, unsigned long &previousMillis, void (VolvoDIM::*setBlinker)(int))
  {
    unsigned long currentMillis = millis();

    if (currentTurn != previousTurn)
    {
      previousMillis = currentMillis;
      previousTurn = currentTurn;
      blinkerState = (currentTurn == 1);
      lastBlinkMillis = currentMillis;
    }

    if (currentTurn == 1)
    {
      if (currentMillis - lastBlinkMillis >= blinkInterval)
      {
        blinkerState = !blinkerState;
        (VolvoDIM.*setBlinker)(blinkerState ? 1 : 0);
        lastBlinkMillis = currentMillis;
      }
    }
    else
    {
      blinkerState = false;
      (VolvoDIM.*setBlinker)(0);
    }
  }

  // Send custom CAN message
  void sendCustomCANMessage(unsigned long canId, unsigned char *data, int dataLength = 8)
  {
    CAN.sendMsgBuf(canId, 1, dataLength, data);
  }

  // Custom odometer display function using direct CAN messages
  void setOdometer(unsigned long mileage)
  {
    if (!odometerEnabled || mileage == lastOdometerValue)
    {
      return; // Don't update if disabled or value hasn't changed
    }

    lastOdometerValue = mileage * 1000;

    // Example: Send odometer value to a specific CAN ID
    // You'll need to adjust the CAN ID and data format based on your DIM requirements
    unsigned long odometerCanId = 0x1A6; // Example CAN ID - adjust as needed
    unsigned char odometerData[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // Pack mileage into CAN data bytes
    // This is an example - you'll need to adjust based on your DIM's expected format
    odometerData[0] = (mileage >> 24) & 0xFF; // Most significant byte
    odometerData[1] = (mileage >> 16) & 0xFF;
    odometerData[2] = (mileage >> 8) & 0xFF;
    odometerData[3] = mileage & 0xFF; // Least significant byte
    odometerData[4] = 0x00;           // Additional data if needed
    odometerData[5] = 0x00;
    odometerData[6] = 0x00;
    odometerData[7] = 0x00;

    // Send the custom CAN message
    sendCustomCANMessage(odometerCanId, odometerData);

    // Optional: Add delay to prevent flooding
    delay(10);
  }

  // Alternative method: Send odometer as text display
  void setOdometerAsText(unsigned long mileage)
  {
    if (!odometerEnabled || mileage == lastOdometerValue)
    {
      return;
    }

    lastOdometerValue = mileage;

    // Format mileage as string
    String mileageStr = String(mileage);
    while (mileageStr.length() < 6)
    {
      mileageStr = " " + mileageStr;
    }
    mileageStr += " mi";

    // Use VolvoDIM's custom text function
    VolvoDIM.setCustomText(mileageStr.c_str());
  }

  // Enable/disable odometer display
  void enableOdometer(bool enable)
  {
    odometerEnabled = enable;
    if (!enable)
    {
      // Clear the display when disabled
      lastOdometerValue = 0;
    }
  }

public:
  /*
    CUSTOM PROTOCOL CLASS
    SEE https://github.com/zegreatclan/SimHub/wiki/Custom-Arduino-hardware-support

    GENERAL RULES :
    - ALWAYS BACKUP THIS FILE, reinstalling/updating SimHub would overwrite it with the default version.
    - Read data AS FAST AS POSSIBLE in the read function
    - NEVER block the arduino (using delay for instance)
    - Make sure the data read in "read()" function READS ALL THE DATA from the serial port matching the custom protocol definition
    - Idle function is called hundreds of times per second, never use it for slow code, arduino performances would fall
    - If you use library suspending interrupts make sure to use it only in the "read" function when ALL data has been read from the serial port.
      It is the only interrupt safe place

    COMMON FUNCTIONS :
    - FlowSerialReadStringUntil('\n')
      Read the incoming data up to the end (\n) won't be included
    - FlowSerialReadStringUntil(';')
      Read the incoming data up to the separator (;) separator won't be included
    - FlowSerialDebugPrintLn(string)
      Send a debug message to simhub which will display in the log panel and log file (only use it when debugging, it would slow down arduino in run conditions)

  */

  // Called when starting the arduino (setup method in main sketch)
  void setup()
  {
    VolvoDIM.gaugeReset();
    VolvoDIM.init();
    VolvoDIM.enableSerialErrorMessages();
    enableOdometer(true); // Enable odometer display by default
  }

  // Called when new data is coming from computer
  void read()
  {
    // Protocol format:
    // [WaterTemperature],[SpeedMph],[Rpms],[Fuel_Percent],[OilTemperature],[Gear],[CurrentDateTime],[SessionOdo],[GameVolume],[RPMShiftLight1],[Brake],[OpponentsCount],[TurnIndicatorRight],[TurnIndicatorLeft],[TotalOdometer]

    int waterTemp = floor(FlowSerialReadStringUntil(',').toInt() * .72);   // 1 - WaterTemperature (converted for coolant)
    int carSpeed = FlowSerialReadStringUntil(',').toInt();                 // 2 - SpeedMph
    int rpms = FlowSerialReadStringUntil(',').toInt();                     // 3 - Rpms
    int fuelPercent = FlowSerialReadStringUntil(',').toInt();              // 4 - Fuel_Percent
    int oilTemp = FlowSerialReadStringUntil(',').toInt();                  // 5 - OilTemperature
    String gear = FlowSerialReadStringUntil(',');                          // 6 - Gear
    String currentDateTime = FlowSerialReadStringUntil(',');               // 7 - CurrentDateTime (format: "3/6/2025 05:45:34 PM")
    int sessionOdo = FlowSerialReadStringUntil(',').toInt();               // 8 - SessionOdo
    int gameVolume = FlowSerialReadStringUntil(',').toInt();               // 9 - GameVolume
    int rpmShiftLight = FlowSerialReadStringUntil(',').toInt();            // 10 - RPMShiftLight1
    int brake = FlowSerialReadStringUntil(',').toInt();                    // 11 - Brake
    int opponentsCount = FlowSerialReadStringUntil(',').toInt();           // 12 - OpponentsCount
    int rightTurn = FlowSerialReadStringUntil(',').toInt();                // 13 - TurnIndicatorRight
    int leftTurn = FlowSerialReadStringUntil('\n').toInt();                 // 14 - TurnIndicatorLeft
    unsigned long totalOdometer = FlowSerialReadStringUntil('\n').toInt(); // 15 - TotalOdometer

    // Parse date/time and set clock
    int hour = 12, minute = 0, ampm = 0;
    parseDateTime(currentDateTime, hour, minute, ampm);
    int timeValue = VolvoDIM.clockToDecimal(hour, minute, ampm);
    VolvoDIM.setTime(timeValue);

    // Handle blinkers with timing
    handleBlinker(leftTurn, leftBlinkerState, lastBlinkMillisLeft, previousLeftTurn, previousMillisLeft, &VolvoDIM::setLeftBlinker);
    handleBlinker(rightTurn, rightBlinkerState, lastBlinkMillisRight, previousRightTurn, previousMillisRight, &VolvoDIM::setRightBlinker);

    // Update VolvoDIM gauges
    VolvoDIM.setOutdoorTemp(oilTemp);        // Set oil temperature as outdoor temp
    VolvoDIM.setCoolantTemp(waterTemp);      // Set water/coolant temperature
    VolvoDIM.setSpeed(carSpeed);             // Set speedometer in mph
    VolvoDIM.setGasLevel(fuelPercent);       // Set fuel gauge 0-100
    VolvoDIM.setRpm(rpms);                   // Set tachometer
    VolvoDIM.setGearPosText(gear.charAt(0)); // Set gear position

    // Set custom odometer display using direct CAN message
    //setOdometer(totalOdometer); // Display total odometer mileage via custom CAN

    // Alternative: Use text display for odometer
    // setOdometerAsText(totalOdometer);        // Display as text in message area

    // Use session odometer for mileage tracking (if needed for other purposes)
    VolvoDIM.enableMilageTracking(sessionOdo > 0 ? 1 : 0);

    VolvoDIM.enableDisableDingNoise(gameVolume > 0 ? 0 : 0); // Enable ding based on game volume

    // Set brightness based on shift light (you can adjust this logic)
    int brightness = map(rpmShiftLight, 0, 8000, 50, 256);
    VolvoDIM.setTotalBrightness(brightness);
    if (rightTurn == 1)
    {
      VolvoDIM.setRightBlinker(1); // Set right blinker on
    }
    else
    {
      VolvoDIM.setRightBlinker(0); // Set right blinker off
    }
    if (leftTurn == 1)
    {
      VolvoDIM.setLeftBlinker(1); // Set left blinker on
    }
    else
    {
      VolvoDIM.setLeftBlinker(0); // Set left blinker off
    }
    // You can use brake and opponentsCount for additional features if needed
    // VolvoDIM.enableBrake(brake);
  }

  // Called once per arduino loop, timing can't be predicted,
  // but it's called between each command sent to the arduino
  void loop()
  {
    VolvoDIM.simulate();
  }

  // Called once between each byte read on arduino,
  // THIS IS A CRITICAL PATH :
  // AVOID ANY TIME CONSUMING ROUTINES !!!
  // PREFER READ OR LOOP METHOS AS MUCH AS POSSIBLE
  // AVOID ANY INTERRUPTS DISABLE (serial data would be lost!!!)
  void idle()
  {
  }
};

#endif
