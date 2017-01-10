#include "PM_dev.h"
#include "Stopsign.h"

#ifndef _PM_dev_dropouts_h_
#define _PM_dev_dropouts_h_

//-----------------------------------------------------------------------------------
// PM_dev_dropouts class
//    This class extends the PM_dev class to add 'rowing dropout' capability.  This
//    supports a physical setup where a switch is closed when the rowing machine
//    is properly 'balanced', and the switch is open when the machine is
//    'unbalanced'.
//
//    When 'unbalanced', the rower is not supposed to receive credit for rowing, 
//    hence it is a 'dropout'.  During a dropout, this class accumulates meters 
//    abd calories.  During a workout, the displayed meters and calories is always
//    the value reported from the PM minus the accumulation during the dropouts
//    (meters counting down is handled appropriately).  The accumulators are
//    reset to zero when the workout ends.
//
//    During a dropout, the rower will see that his meters and calories are not
//    updating.  He will also see watts and pace displayed as zero.  And he will
//    also see a red stopsign - the stopsign appears even when a workout is not
//    in progress.

class PM_dev_dropouts : public PM_dev
{
  boolean dropout;
  uint16_t accumCalories;
  int32_t accumMeters;
  
  // Input pin used to 'disallow' rowing updates
  int inputPin;

  Stopsign stopsign;
  sdfadfafd
public:
  //----------------------------------------------------------------------------------
  // Constructor - inputPin is used the 'disallow' rowing updates
  PM_dev_dropouts(PM_usb* p, int inputPin) : PM_dev(p)
  {
    this->inputPin = inputPin;
    pinMode(inputPin, INPUT); 
    digitalWrite(inputPin, HIGH);  // pull up

    dropout       = false;    
    accumCalories = 0;
    accumMeters   = 0;
  }

  // Override these from the base class
  virtual uint16_t getCalories() {return PM_dev::getCalories() - accumCalories;}
  virtual uint32_t getMeters()   {return PM_dev::getMeters() - accumMeters;}
  virtual uint16_t getPace()     {return (dropout ? 0 : PM_dev::getPace());}
  virtual uint16_t getWatts()    {return (dropout ? 0 : PM_dev::getWatts());}
    
  //----------------------------------------------------------------------------------
  // PM_Usb::PM_poller::Poll() implementation - overridden from base class
  virtual void Poll(bool okToPoll)
  {
    // Get a copy of the previous values
    uint16_t calories0 = PM_dev::getCalories();
    int32_t meters0 = (int32_t)PM_dev::getMeters();

    // Get the latest values from the PM
    PM_dev::Poll(okToPoll);

    // Determine if we are in a dropout (switch open) condition
    dropout = (HIGH == digitalRead(inputPin));
    if (dropout)
    {
      accumCalories += (PM_dev::getCalories() - calories0);
      accumMeters += (((int32_t)PM_dev::getMeters()) - meters0);
    }

    // Draw/erase the stopsign
    stopsign.draw(dropout);

    // STATE_READY is when the menu is displayed, or a workout has been selected but not started    
    if (PM_dev::getSlaveState() == STATE_READY)
    {
      accumCalories = 0;
      accumMeters = 0;
    }
  }
};

#endif

