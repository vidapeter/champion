#include <SPI.h>


#include "Usb.h"
#include "usbhub.h"

#include "PM_usb.h"
#include "PM_dev.h"
#include "PM_units.h"
#include "display.h"

// The main objects
USB     Usb;
PM_usb  PmUsb(&Usb);

// Change this to a 0 (zero) to exclude the 'dropout' (disable rowing) functionality
#define INCLUDE_ROWING_DROPOUTS 0
#if INCLUDE_ROWING_DROPOUTS
#include "PM_dev_dropouts.h"
PM_dev_dropouts PmDev(&PmUsb, 5); // 2nd argument is the input pin number
#else
PM_dev  PmDev(&PmUsb);
#endif

Display display; 

// Declare these Hubs in case hubs exist in the external connections.
// No other code is necessary beyond these declarations.  Note that
// these hubs show up as devices within the USB class device structures.
#if 1
USBHub  Hub1(&Usb);
USBHub  Hub2(&Usb);
USBHub  Hub3(&Usb);
USBHub  Hub4(&Usb);
USBHub  Hub5(&Usb);
USBHub  Hub6(&Usb);
USBHub  Hub7(&Usb);
#endif

// Input switches - we pull 'c' low, and enable the pullups on '1' and '2', then
// the switch will connect '1' and/or '2' to 'c'
const int switch1 = 2;
const int switchc = 4;
const int switch2 = 6;

void setup()
{
  //delay(1000);
  // Serial is used for debug output only
  Serial.begin(115200);
  Serial.println("Start");

  // Setup the SPI select pins so the boards don't interfere during initialization
  // Gameduino SS_PIN 9
  // USB       SS_PIN 10
  P9::SetDirWrite();
  P9::Set();
  P10::SetDirWrite();
  P10::Set();

  // Now we can initialize the Gameduino with our graphics content
  display.begin();
  display.setColors(RGB(255,255,255), // foreground
                    RGB(127,127,127), // antialias
                    RGB(0,0,0));      // background

  // For our Freeduino with Usb Host, we have to de-assert the hardware reset pin
  // for the usb chip (the Usb library has no knowledge of this pin)
  P7::SetDirWrite();
  P7::Set();

  Usb.begin();  
  if (Usb.Init() == -1)
      Serial.println("Usb.Init failed: OSC did not start.");

  // Setup the switch inputs
  pinMode(switch1, INPUT); 
  pinMode(switchc, OUTPUT);
  pinMode(switch2, INPUT); 
  digitalWrite(switchc, LOW);   // pull down to 0v.
  digitalWrite(switch1, HIGH);  // enables the pullups
  digitalWrite(switch2, HIGH);  
}

DisplayUnitsEnum GetUnits()
{
  int sw1 = digitalRead(switch1);
  int sw2 = digitalRead(switch2);
  if (sw2 == HIGH)
    if (sw1 == HIGH)
      return Units_Meters;
    else
      return Units_Power;
  else
    if (sw1 == HIGH)
      return Units_Work;
    else
      return Units_Pace;
}

void loop()
{
  // Perform any USB enumeration, setup, etc, including polling
  // the PM for data
  Usb.Task();

  // Display an indication that the PM is not there, or the data
  GD.waitvblank();
  if (!PmDev.getDataIsValid()) 
  {
    display.drawSearching();
    // sleep a short time so we can check the bus again quickly
    delay(100);
  }
  else
  {
    switch (GetUnits())
    {
      case Units_Meters:
        display.drawNumber(PmDev.getMeters(), METERS_FRAME0);
      break;
      case Units_Power:
        display.drawNumber(PmDev.getWatts(), WATTS_FRAME0);
      break;
      case Units_Work:
        display.drawNumber(PmDev.getCalories(), CAL_FRAME0);
      break;
      case Units_Pace:
        display.drawTime(PmDev.getPace());
      break;
    }
    delay(500); // use same update rate as the PM display
  }
}
 

