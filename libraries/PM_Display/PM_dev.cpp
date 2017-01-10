#include "PM_dev.h"

// PM CSAFE messaging constants

#define CSAFE_GETVERSION_CMD          0x91
#define CSAFE_GETCALORIES_CMD         0xA3
#define CSAFE_GETPACE_CMD             0xA6
#define CSAFE_GETPOWER_CMD            0xB4

// PM-specific commands - CFG1 is followed by length then the PM-specific command
#define CSAFE_SETUSERCFG1_CMD         0x1A
#define CSAFE_PM_SPECIFIC_FLAG        CSAFE_SETUSERCFG1_CMD
#define CSAFE_PM_GET_WORKDISTANCE     0xA3
#define CSAFE_PM_GET_WORKOUTTYPE      0x89

// PM framing protocol constants
#define PM_EXTENDED_START_FLAG        0xF0
#define PM_STANDARD_START_FLAG        0xF1
#define PM_STOP_FLAG                  0xF2
#define PM_ESCAPE_FLAG                0xF3

#define PM_ESCAPE_EXTENDED_START_FLAG 0x00
#define PM_ESCAPE_STANDARD_START_FLAG 0x01
#define PM_ESCAPE_STOP_FLAG           0x02
#define PM_ESCAPE_ESCAPE_FLAG         0x03

#define SLAVE_STATE_MASK         0x0f
#define PREV_FRAME_STATUS_MASK   0x30
#define FRAME_COUNT_MASK         0X80

#define GET_SLAVE_STATE(val)     (val & SLAVE_STATE_MASK)
#define GET_FRAME_STATUS(val)    ((val & PREV_FRAME_STATUS_MASK) >> 4)
#define GET_FRAME_COUNT(val)     ((val & FRAME_COUNT_MASK) >> 7)

enum SLAVE_STATUSES {
   STATUS_OK,
   STATUS_PREV_REJECT,
   STATUS_PREV_BAD,
   STATUS_PREV_NOT_READY
};

//---------------------------------------------------------------------------------
// Constructor - initialize and then register with the PM_usb device
PM_dev::PM_dev(PM_usb* p) : pPmUsb(p)
{
  dataIsValid      = false;
  metersDecreasing = false;
  pPmUsb->RegisterPmPoller(this);
  
  // Pre-build the big poll command (which is constant)
  BuildPollCommand();
}

//------------------------------------------------------------------------
// Build our polling command
void PM_dev::BuildPollCommand()
{
  for (int i = 0; i < sizeof(cmdBuf); i++)
    cmdBuf[i] = 0;
  cmdLen = 0;
  uint8_t *cmdBufPtr = &cmdBuf[0];

  *cmdBufPtr++ = PM_REPORT_ID;
  *cmdBufPtr++ = PM_STANDARD_START_FLAG;

  uint8_t *ptr0 = cmdBufPtr;

  *cmdBufPtr++ = CSAFE_PM_SPECIFIC_FLAG;
  *cmdBufPtr++ = 2; // the number of pm-specific command bytes to follow
  *cmdBufPtr++ = CSAFE_PM_GET_WORKOUTTYPE;
  *cmdBufPtr++ = CSAFE_PM_GET_WORKDISTANCE;
  
  *cmdBufPtr++ = CSAFE_GETCALORIES_CMD;
  *cmdBufPtr++ = CSAFE_GETPACE_CMD;
  *cmdBufPtr++ = CSAFE_GETPOWER_CMD;
  
  *cmdBufPtr++ = Checksum(ptr0, cmdBufPtr - ptr0);
  
  cmdBufPtr = Stuff(ptr0, cmdBufPtr - ptr0);
  
  *cmdBufPtr++ = PM_STOP_FLAG;
  
  cmdLen = cmdBufPtr - &cmdBuf[0];
}

//--------------------------------------------------------------------------
// Return the checksum for the given buffer of data  
uint8_t PM_dev::Checksum(uint8_t *where, uint8_t len)
{
  uint8_t sum = 0;
  for (int i = 0; i < len; i++)
    sum ^= *where++; 
  return sum;
}
  
//--------------------------------------------------------------------------
// Copy bytes from 'src' to 'dest', using the PM 'byte stuffing' as 
// necessary.
uint8_t* PM_dev::Stuff(uint8_t *where, uint8_t len)
{
  // Find out how many extra bytes we will need
  uint8_t extra = 0;
  for (int i = 0; i < len; i++)
  {
    switch(where[i])
    {
      case PM_EXTENDED_START_FLAG:
      case PM_STANDARD_START_FLAG:
      case PM_STOP_FLAG:
      case PM_ESCAPE_FLAG:
        extra++;
      break;
      default:
      break;
    }
  }

  // The first byte after the stuffing has been completed...  
  uint8_t *after = where + len + extra;
  
  // Start at the end
  for (int i = len - 1; i >= 0; i--)
  {
    uint8_t x = where[i];
    switch(x)
    {
      case PM_EXTENDED_START_FLAG:
        where[i + extra] = PM_ESCAPE_EXTENDED_START_FLAG;
        extra--;
        where[i + extra] = PM_ESCAPE_FLAG;
      break;
      case PM_STANDARD_START_FLAG:
        where[i + extra] = PM_ESCAPE_STANDARD_START_FLAG;
        extra--;
        where[i + extra] = PM_ESCAPE_FLAG;
      break;
      case PM_STOP_FLAG:
        where[i + extra] = PM_ESCAPE_STOP_FLAG;
        extra--;
        where[i + extra] = PM_ESCAPE_FLAG;
      break;
      case PM_ESCAPE_FLAG:
        where[i + extra] = PM_ESCAPE_ESCAPE_FLAG;
        extra--;
        where[i + extra] = PM_ESCAPE_FLAG;
      break;
      default:
        where[i + extra] = x;
      break;
    }
  }
  
  return after;
}

//----------------------------------------------------------------------------
// Remove all 'bytes-stuffing' from the rspBuf (all processing done in-place)
uint8_t PM_dev::Unstuff()
{
  uint8_t *src  = rspBuf;
  uint8_t *dest = rspBuf;
  while ((src - rspBuf) < sizeof(rspBuf))
  {
    uint8_t x = *src++;
    if (x == PM_ESCAPE_FLAG)
    {
      x = *src++;
      switch(x)
      {
        case PM_ESCAPE_EXTENDED_START_FLAG:
          *dest++ = PM_EXTENDED_START_FLAG;
        break;
        case PM_ESCAPE_STANDARD_START_FLAG:
          *dest++ = PM_STANDARD_START_FLAG;
        break;
        case PM_ESCAPE_STOP_FLAG:
          *dest++ = PM_STOP_FLAG;
        break;
        case PM_ESCAPE_ESCAPE_FLAG:
          *dest++ = PM_ESCAPE_FLAG;
        break;
        default:
          *dest++ = x;
        break;
      }
    }
    else
    {
      *dest++ = x;
      if (x == PM_STOP_FLAG) break;
    }
  }
  return (dest - rspBuf);
}

//----------------------------------------------------------------------------
// Extrat a 16-bit unsigned integer  
uint16_t PM_dev::GetUint16(uint8_t *ptr)
{
  uint16_t x = (((uint16_t)ptr[0]) << 0) + 
               (((uint16_t)ptr[1]) << 8);
  return x;
}

//----------------------------------------------------------------------------
// Extract a 32-bit unsigned integer
uint32_t PM_dev::GetUint32(uint8_t *ptr)
{
  uint32_t x = (((uint32_t)ptr[0]) << 0) +
               (((uint32_t)ptr[1]) << 8) +
               (((uint32_t)ptr[2]) << 16) +
               (((uint32_t)ptr[3]) << 24);
  return x;
}

//----------------------------------------------------------------------------
// Send the poll command the read the response into rspBuf  
uint8_t PM_dev::SendCommandAndGetResponse(uint8_t *count)
{
  #if 0
  Notify(PSTR("send: "));
  for (uint8_t i = 0; i < cmdLen; i++)
  {
    PrintHex<uint8_t>(cmdBuf[i]);
    Serial.print(" ");
  }
  Serial.println("");
  #endif
  
  // Send command and get response
  uint16_t rspLen;
  uint8_t  rcode;

  rcode = pPmUsb->SendCommand(cmdBuf, PM_REPORT_LEN);
  if (rcode != 0) return rcode;

  rspLen = PM_REPORT_LEN;
  rcode = pPmUsb->GetResponse(rspBuf, &rspLen);
  if (rcode != 0) return rcode;
  
  if (rspLen != PM_REPORT_LEN)
  {
    return 0;
  }

  // Parse the response
  *count = Unstuff();
  #if 0
  Notify(PSTR("recv: "));
  for (uint8_t i = 0; i < count; i++)
  {
    PrintHex<uint8_t>(rspBuf[i]);
    Serial.print(" ");
  }
  Serial.println("");
  #endif
  
  return 0;  
}

//---------------------------------------------------------------------------------
// PM_poller::Poll() implementation
//    At every invocation, this function polls the PM for all data and 
//    saves the results.
void PM_dev::Poll(bool okToPoll)
{
  this->dataIsValid = false;
  if (!okToPoll)
    return;

  uint8_t count;
  if (0 != SendCommandAndGetResponse(&count)) return;
  
  uint16_t index = 0;
  
  // First byte should be the report ID
  if (rspBuf[index++] != PM_REPORT_ID)
  {
    return;
  }
  
  // Next byte should be the start flag
  if (rspBuf[index++] != PM_STANDARD_START_FLAG)
  {
    return;
  }
  
  // Next byte should be the status byte
  uint8_t statusByte = rspBuf[index++];
  slaveState = (SLAVE_STATES)GET_SLAVE_STATE(statusByte);
  switch (slaveState)
  {
    case STATE_ERROR:    Notify(PSTR("STATE_ERROR"));    break;
    case STATE_READY:    Notify(PSTR("STATE_READY"));    break;
    case STATE_IDLE:     Notify(PSTR("STATE_IDLE"));     break;
    case STATE_HAVEID:   Notify(PSTR("STATE_HAVEID"));   break;
    case STATE_INUSE:    Notify(PSTR("STATE_INUSE"));    break;
    case STATE_PAUSED:   Notify(PSTR("STATE_PAUSED"));   break;
    case STATE_FINISHED: Notify(PSTR("STATE_FINISHED")); break;
    case STATE_MANUAL:   Notify(PSTR("STATE_MANUAL"));   break;
    case STATE_OFFLINE:  Notify(PSTR("STATE_OFFLINE"));  break;
    default:             Notify(PSTR("STATE unknown ")); 
                         Serial.print(slaveState, DEC); 
    break;
  }
  
  // The rest contains the command responses
  bool    pmSpecific = false;
  uint8_t indexAfterPmSpecific = 0;
  bool    parseError = false;
  uint8_t lastIndex = count - 2; // don't parse checksum or STOP flag

  while (!parseError && (index < lastIndex))
  {
    if ((pmSpecific) && (index >= indexAfterPmSpecific))
      pmSpecific = false;

    uint8_t id = rspBuf[index++];
    if (index > lastIndex) {parseError = true; break;}

    if (id == CSAFE_PM_SPECIFIC_FLAG)
    {
      pmSpecific = true;
      indexAfterPmSpecific = index + rspBuf[index];

      index++;
      if (index > lastIndex) {parseError = true; break;}

      id = rspBuf[index++];
      if (index > lastIndex) {parseError = true; break;}
    }
    
    uint8_t byteCount = rspBuf[index++];
    uint8_t *data = &rspBuf[index];

    if (byteCount == 0)
      Notify(PSTR("\r\nbyteCount is zero\r\n"));

    index += byteCount;
    if (index > lastIndex) {parseError = true; break;}

    if (pmSpecific)
    {
      switch(id)
      {
        case CSAFE_PM_GET_WORKDISTANCE:
          parseError = !ParseMeters(data, byteCount);
          Notify(PSTR(" : m ")); Serial.print(this->meters, DEC); 
          Serial.print('.'); Serial.print(this->metersFrac, DEC);
        break;
        case CSAFE_PM_GET_WORKOUTTYPE:
          parseError = !ParseWorkoutType(data, byteCount);
          Notify(PSTR(" : type ")); Serial.print(workoutType, DEC);
        break;
        default:
        break;
      }
    }
    else
    {
      switch(id)
      {
        case CSAFE_GETCALORIES_CMD:
          parseError = !ParseCalories(data, byteCount);
          Notify(PSTR(" : cal ")); Serial.print(calories, DEC);
        break;
        case CSAFE_GETPACE_CMD:
          parseError = !ParsePace(data, byteCount);
          Notify(PSTR(" : pace ")); Serial.print(pace, DEC);
        break;
        case CSAFE_GETPOWER_CMD:
          parseError = !ParseWatts(data, byteCount);
          Notify(PSTR(" : watts ")); Serial.print(watts, DEC);
        break;
        case CSAFE_GETVERSION_CMD:
          parseError = !ParsePmVersion(data, byteCount);
        break;
        default:
        break;
      }
    }
  }
  // Special processing - the PM doesn't zero out the workout numbers when it goes
  // from the workout to the main menu, but we want the number displayed to be zero.
  // STATE_READY is the state when the a workout is not in progress - either
  // the main menu is displayed on the PM, or a workout has been selected but not
  // started.  For workout types >= 2, the PM will be in a countdown mode so it
  // is displaying the non-zero workout to be performed.
  if ((slaveState == STATE_READY) && (workoutType < 2))
  {
    calories   = 0;
    meters     = 0;
    metersFrac = 0;
    pace       = 0;
    watts      = 0;
  }
    
  dataIsValid = !parseError;
  Serial.println(' ');
}

bool PM_dev::ParseCalories(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 2) return false;
  calories = GetUint16(&rsp[0]);
  return true;
}

bool PM_dev::ParseMeters(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 5) return false;

  uint32_t m = GetUint32(&rsp[0]);
  // The PM uses 0.1 m resolution, so we have to divide by 10 to get meters
  m = m / 10;
  metersFrac = rsp[4];
  // Meters requires more special handling - the PM, when in a count-down mode, sends us
  // one less than the 'meters' that it displays, except at the beginning and end of
  // the exercise.  When counting up, is sends us the correct value.  So, we determine
  // which way meters is changing, and apply the adjustment as necessary.
  if (m != metersFromPm)
    metersDecreasing = (m < metersFromPm); 
  metersFromPm = m;
  if (metersDecreasing && (metersFromPm != 0))
  {
    meters = metersFromPm + 1;
  }
  else
  {
    meters = metersFromPm;
  }
  return true;
}

bool PM_dev::ParsePace(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 3) return false;
  pace = GetUint16(&rsp[0]);
  // The PM reports sec/km and we want sec/500m - make sure we round up
  pace = (pace + 1) / 2;
  // Ignore the third byte 'units'
  return true;
}

bool PM_dev::ParsePmVersion(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 7) return false;
  pmVersion.mfg   = rsp[0];
  pmVersion.cid   = rsp[1];
  pmVersion.model = rsp[2];
  pmVersion.hw    = GetUint16(&rsp[3]);
  pmVersion.sw    = GetUint16(&rsp[5]);
  return true;
}

bool PM_dev::ParseWatts(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 3) return false;
  watts = GetUint16(&rsp[0]);
  // Ignore the third byte, units
  return true;
}

bool PM_dev::ParseWorkoutType(uint8_t *rsp, uint8_t len)
{
  if (len == 0) return true;
  if (len != 1) return false;
  workoutType = rsp[0];
  return true;
}
  

