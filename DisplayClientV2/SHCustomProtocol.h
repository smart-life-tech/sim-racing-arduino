#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>
#include <VolvoDIM.h>

VolvoDIM VolvoDIM(9, 6);

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

  // Function to parse date/time string and extract hour/minute/AM-PM
  void parseDateTime(String dateTimeStr, int &hour, int &minute, int &ampm)
  {
    // Format: "3/6/2025 05:45:34 PM"
    // Find the space before the time
    int spaceIndex = dateTimeStr.indexOf(' ');
    if (spaceIndex == -1) return;
    
    // Extract time portion: "05:45:34 PM"
    String timeStr = dateTimeStr.substring(spaceIndex + 1);
    
    // Find AM/PM
    ampm = 0; // Default to AM
    if (timeStr.indexOf("PM") != -1) {
      ampm = 1;
    }
    
    // Extract hour and minute
    int firstColon = timeStr.indexOf(':');
    int secondColon = timeStr.indexOf(':', firstColon + 1);
    
    if (firstColon != -1 && secondColon != -1) {
      hour = timeStr.substring(0, firstColon).toInt();
      minute = timeStr.substring(firstColon + 1, secondColon).toInt();
      
      // Convert 24-hour to 12-hour format if needed
      if (hour == 0) {
        hour = 12; // Midnight becomes 12 AM
      } else if (hour > 12) {
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
  }

  // Called when new data is coming from computer
  void read()
  {
    // Protocol format:
    // [WaterTemperature],[SpeedMph],[Rpms],[Fuel_Percent],[OilTemperature],[Gear],[CurrentDateTime],[SessionOdo],[GameVolume],[RPMShiftLight1],[Brake],[OpponentsCount],[TurnIndicatorRight],[TurnIndicatorLeft]
    
    int waterTemp = floor(FlowSerialReadStringUntil(',').toInt() * .72);    // 1 - WaterTemperature (converted for coolant)
    int carSpeed = FlowSerialReadStringUntil(',').toInt();                  // 2 - SpeedMph
    int rpms = FlowSerialReadStringUntil(',').toInt();                      // 3 - Rpms
    int fuelPercent = FlowSerialReadStringUntil(',').toInt();               // 4 - Fuel_Percent
    int oilTemp = FlowSerialReadStringUntil(',').toInt();                   // 5 - OilTemperature
    String gear = FlowSerialReadStringUntil(',');                           // 6 - Gear
    String currentDateTime = FlowSerialReadStringUntil(',');                // 7 - CurrentDateTime (format: "3/6/2025 05:45:34 PM")
    int sessionOdo = FlowSerialReadStringUntil(',').toInt();                // 8 - SessionOdo
    int gameVolume = FlowSerialReadStringUntil(',').toInt();                // 9 - GameVolume
    int rpmShiftLight = FlowSerialReadStringUntil(',').toInt();             // 10 - RPMShiftLight1
    int brake = FlowSerialReadStringUntil(',').toInt();                     // 11 - Brake
    int opponentsCount = FlowSerialReadStringUntil(',').toInt();            // 12 - OpponentsCount
    int rightTurn = FlowSerialReadStringUntil(',').toInt();                 // 13 - TurnIndicatorRight
    int leftTurn = FlowSerialReadStringUntil('\n').toInt();                 // 14 - TurnIndicatorLeft

    // Parse date/time and set clock
    int hour = 12, minute = 0, ampm = 0;
    parseDateTime(currentDateTime, hour, minute, ampm);
    int timeValue = VolvoDIM.clockToDecimal(hour, minute, ampm);
    VolvoDIM.setTime(timeValue);

    // Handle blinkers with timing
    handleBlinker(leftTurn, leftBlinkerState, lastBlinkMillisLeft, previousLeftTurn, previousMillisLeft, &VolvoDIM::setLeftBlinker);
    handleBlinker(rightTurn, rightBlinkerState, lastBlinkMillisRight, previousRightTurn, previousMillisRight, &VolvoDIM::setRightBlinker);

    // Update VolvoDIM gauges
    VolvoDIM.setOutdoorTemp(oilTemp);           // Set oil temperature as outdoor temp
    VolvoDIM.setCoolantTemp(waterTemp);         // Set water/coolant temperature
    VolvoDIM.setSpeed(carSpeed);                // Set speedometer in mph
    VolvoDIM.setGasLevel(fuelPercent);          // Set fuel gauge 0-100
    VolvoDIM.setRpm(rpms);                      // Set tachometer
    VolvoDIM.setGearPosText(gear.charAt(0));    // Set gear position
    VolvoDIM.enableMilageTracking(sessionOdo);  // Set odometer
    VolvoDIM.enableDisableDingNoise(gameVolume > 0 ? 1 : 0); // Enable ding based on game volume
    
    // Set brightness based on shift light (you can adjust this logic)
    int brightness = map(rpmShiftLight, 0, 8000, 50, 256);
    VolvoDIM.setTotalBrightness(brightness);
    
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
