#include "message.h"
#include "PM_usb.h"

#ifndef _PM_dev_h_
#define _PM_dev_h_

//-----------------------------------------------------------------------------------
// PM_dev class
//    This class implements the message-level communications with the PM usb device.
//    In the constructor, it registers itself with the given PM_usb object.  Thereafter
//    this class expects its Poll() function to be called periodically by the PM_usb
//    object (which is in turn called by the USB object).  This Poll() function 
//    polls for all the relevant data from the PM.

// Limitations:
//   - This code uses only the 'standard frame' not the 'extended frame' - because there is no
//     need for the addressing capability when we have point-to-point usb links
//   - This code uses only a handful of GET commands

class PM_dev : public PM_usb::PM_poller
{
  PM_usb *pPmUsb;
  
  // Use the mid-sized report
  #define PM_REPORT_ID  (4)
  #define PM_REPORT_LEN (1 + 62)

  uint8_t cmdBuf[PM_REPORT_LEN];
  uint8_t rstBuf[PM_REPORT_LEN];
  uint8_t cmdLen;
  uint8_t rstLen;
  uint8_t rspBuf[PM_REPORT_LEN];
  

  // Fill cmdBuf with the poll command
  void BuildPollCommand();


  // Return the checksum for the given buffer of data  
  uint8_t Checksum(uint8_t *where, uint8_t len);

  // Perform byte stuffing
  uint8_t* Stuff(uint8_t *where, uint8_t len);

  // Remove all 'bytes-stuffing' from the rspBuf - returns the number of bytes
  // after unstuffing.
  uint8_t Unstuff();

  // Extract 16-bit unsigned integer  
  uint16_t GetUint16(uint8_t *ptr);
  
  // Extract 32-bit unsigned integer
  uint32_t GetUint32(uint8_t *ptr);
  
public:
  //----------------------------------------------------------------------------------
  // Constructor - initialize and then register with the PM_usb device
  PM_dev(PM_usb* p);

  //----------------------------------------------------------------------------------
  // Accessors - these only change due to a call to Poll() (which happens when
  // due to USB::Task()), so it is safe to assume each one is consistent with
  // dataIsValid.
  enum SLAVE_STATES {
    STATE_ERROR = 1,
    STATE_READY = 2,
    STATE_IDLE = 3,
    STATE_HAVEID = 4,
    STATE_INUSE=5,
    STATE_PAUSED = 6,
    STATE_FINISHED = 7,
    STATE_MANUAL = 8,
    STATE_OFFLINE = 9
  };

  struct PM_Version
  {
    uint8_t  mfg;
    uint8_t  cid;
    uint8_t  model;
    uint16_t hw;
    uint16_t sw;
  };
  
  virtual bool         getDataIsValid() {return dataIsValid;}
  virtual uint16_t     getCalories()    {return calories;}
  virtual uint32_t     getMeters()      {return meters;}
  virtual uint16_t     getPace()        {return pace;}
  virtual PM_Version   getPmVersion()   {return pmVersion;}
  virtual SLAVE_STATES getSlaveState()  {return slaveState;}
  virtual uint16_t     getWatts()       {return watts;}
  virtual uint8_t      getWorkoutType() {return workoutType;}
  // Send the poll command the read the response into rspBuf  
  uint8_t SendCommandAndGetResponse(uint8_t *count);
  //fill cmBuf with the reset command
  void BuildResetCommand();
  uint8_t SendResetCommand(uint8_t *count);

  //----------------------------------------------------------------------------------
  // PM_Usb::PM_poller::Poll() implementation
  //    At every invocation, this function polls the PM for its data.  This function
  //    is invoked by the PM_usb object - this function should not be called by 
  //    any other code.
  virtual void Poll(bool okToPoll);

private:

  // Polled data information
  bool         dataIsValid;
  SLAVE_STATES slaveState;

  uint16_t   calories;
  bool       metersDecreasing;
  uint32_t   metersFromPm;
  uint32_t   meters;
  uint8_t    metersFrac;
  uint16_t   pace;
  PM_Version pmVersion;
  uint16_t   watts;
  uint8_t    workoutType;

  bool ParseCalories(uint8_t *ptr, uint8_t len);
  bool ParseMeters(uint8_t *ptr, uint8_t len);
  bool ParsePace(uint8_t *ptr, uint8_t len);
  bool ParsePmVersion(uint8_t *ptr, uint8_t len);
  bool ParseWatts(uint8_t *ptr, uint8_t len);
  bool ParseWorkoutType(uint8_t *ptr, uint8_t len);
};

#endif

