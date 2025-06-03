#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>
#include <VolvoDIM.h>

VolvoDIM VolvoDIM(9, 6);
int totalBlink = 0, innerLeftBlink = 0, innerRightBlink = 0;
class SHCustomProtocol
{
private:
  bool leftBlinkerState = false;
  bool rightBlinkerState = false;
  unsigned long lastBlinkMillisLeft = 0;
  unsigned long lastBlinkMillisRight = 0;
  int LeftTurn = 0;
  int RightTurn = 0;
  unsigned long previousMillisLeft = 0;
  unsigned long previousMillisRight = 0;
  const unsigned long blinkInterval = 500; // Blink interval in milliseconds
  int previousLeftTurn = 0;
  int previousRightTurn = 0;

  bool engineOn = false; // New variable to track engine state

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
    /* int coolantTemp = floor(FlowSerialReadStringUntil(',').toInt() * .72);
      int carSpeed = FlowSerialReadStringUntil(',').toInt();
      int rpms = FlowSerialReadStringUntil(',').toInt();
      int fuelPercent = FlowSerialReadStringUntil(',').toInt();
      int oilTemp = FlowSerialReadStringUntil('\n').toInt();
    */
    Serial.print("Reading data: ");
    Serial.println(FlowSerialReadStringUntil('\n')); // Debugging line to see incoming data

    int coolantTemp = floor(FlowSerialReadStringUntil(',').toInt() * .72); // 1
    int carSpeed = FlowSerialReadStringUntil(',').toInt();                 // 2
    int rpms = FlowSerialReadStringUntil(',').toInt();                     // 3
    int fuelPercent = FlowSerialReadStringUntil(',').toInt();              // 4
    int oilTemp = FlowSerialReadStringUntil(',').toInt();                  // 5
    String gear = FlowSerialReadStringUntil(',');                          // 6
    int currentLeftTurn = FlowSerialReadStringUntil(',').toInt();          // 7
    int currentRightTurn = FlowSerialReadStringUntil(',').toInt();         // 8
    int hour = FlowSerialReadStringUntil(',').toInt();                     // 9
    int minute = FlowSerialReadStringUntil(',').toInt();                   // 10
    int mileage = FlowSerialReadStringUntil(',').toInt();                  // 11
    int ding = FlowSerialReadStringUntil(',').toInt();                     // 12
    int totalBrightness = FlowSerialReadStringUntil(',').toInt();          // 13
    int highbeam = FlowSerialReadStringUntil(',').toInt();                 // 14
    int fog = FlowSerialReadStringUntil(',').toInt();                      // 15
    int brake = FlowSerialReadStringUntil('\n').toInt();                   // 16

    // int timeValue = VolvoDIM.clockToDecimal(hour,minute,1); //This converts a 12 hour time into a number suitable for setting the clock
    // The format for the function above is hour, minute, am = 0, pm = 1
    // VolvoDIM.setTime(timeValue); //This sets the time on the dim
    /*VolvoDIM.setOutdoorTemp(oilTemp); //This accepts a temperature in fahrenheit -49 - 176 and sets it
      VolvoDIM.setCoolantTemp(coolantTemp); //This sets the coolant gauge 0 - 100
      VolvoDIM.setSpeed(carSpeed); //This sets the spedometer in mph 0-160
      VolvoDIM.setGasLevel(fuelPercent); //This sets the gas guage 0 - 100
      VolvoDIM.setRpm(rpms); //This sets the tachometer 0 - 8000
      VolvoDIM.setTotalBrightness(256); //This sets both of the above brightness's 0 - 256*/
    int timeValue = VolvoDIM.clockToDecimal(hour, minute, 1);

    // Handle left blinker
    handleBlinker(currentLeftTurn, leftBlinkerState, lastBlinkMillisLeft, LeftTurn, previousMillisLeft, &VolvoDIM::setLeftBlinkerSolid);

    // Handle right blinker
    handleBlinker(currentRightTurn, rightBlinkerState, lastBlinkMillisRight, RightTurn, previousMillisRight, &VolvoDIM::setRightBlinkerSolid);

    // Update other gauges
    VolvoDIM.setTime(timeValue);
    VolvoDIM.setOutdoorTemp(oilTemp);
    VolvoDIM.setCoolantTemp(coolantTemp);
    VolvoDIM.setSpeed(carSpeed);
    VolvoDIM.setGasLevel(fuelPercent);
    VolvoDIM.setRpm(rpms);
    VolvoDIM.setGearPosText(gear.charAt(0));
    VolvoDIM.enableMilageTracking(mileage);
    VolvoDIM.enableDisableDingNoise(ding);
    // VolvoDIM.enableHighBeam(highbeam);
    VolvoDIM.setTotalBrightness(totalBrightness);
    // VolvoDIM.enableFog(fog);
    // VolvoDIM.enableBrake(brake);

    /*
      // -------------------------------------------------------
      // EXAMPLE 2 - reads speed and gear from the message
      // Protocol formula must be set in simhub to
      // format([DataCorePlugin.GameData.NewData.SpeedKmh],'0') + ';' + isnull([DataCorePlugin.GameData.NewData.Gear],'N')
      // -------------------------------------------------------

      int speed = FlowSerialReadStringUntil(';').toInt();
      String gear = FlowSerialReadStringUntil('\n');

      FlowSerialDebugPrintLn("Speed : " + String(speed));
      FlowSerialDebugPrintLn("Gear : " + gear);
    */
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
