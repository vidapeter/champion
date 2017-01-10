#include "Usb.h"


#ifndef _PM_usb_h_
#define _PM_usb_h_

//--------------------------------------------------------------------------------
// PM_usb class
//   This class implements the usb-level communications with the PM.  It is based
//   on the HidUniversal code that comes with the USB Host.  Althought the PM
//   is a usb HID device, we don't need any of its HID-specific capability so
//   that layer of the USB protocol is not implemented and we go straight to
//   the USB class.

class PM_usb : public USBDeviceConfig {
  // The control endpoint (0), and two interrupt endpoints
  #define MAX_ENDPOINTS 3

  EpInfo epInfo[MAX_ENDPOINTS];
  struct HIDInterface
  {
    struct
    {
      uint8_t bmInterface : 3;
      uint8_t bmAltSet    : 3;
      uint8_t bmProtocol  : 2;
    };
  } hidInterface;

  uint8_t  bConfNum;
  uint8_t  bNumEP;
  bool     bPollEnable;

protected:
  USB       *pUsb;
  uint8_t    bAddress;

  static const uint8_t epInterruptInIndex;
  static const uint8_t epInterruptOutIndex;

  void Initialize();
  
public:
  PM_usb(USB *pusb);

  //---------------------------------------------------------------------------------------
  // This class provides a way to invoke a higher-level polling capability when the 
  // physical device is available.
  class PM_poller
  {
  public:
    //-------------------------------------------------------------------------------------
    // This method will be called periodically with okToPoll = true while the phsyical 
    // device is plugged in.  When it isn't plugged in, it will be invoked at least once
    // with okToPoll = false.  At startup, the device is not ok to poll.
    virtual void Poll(bool okToPoll) = 0;
  };

protected:
  PM_poller *pPmPoller;

public:

  //---------------------------------------------------------------------------------------
  // Designate a PM_poller object that will be invoked when PM_usb's Poll() method
  // is invoked by the USB object (which occurs in its Task() method).
  void RegisterPmPoller(PM_poller *pPoller)
  {
    pPmPoller = pPoller;
  }

  //---------------------------------------------------------------------------------------
  // The registered PM_poller is expected to call these methods to send a command and
  // get a response.  Typically this happens in the PM_poller's Poll() method.
  // Note the typicially the PM will send the right response - in some cases, if the 
  // transfer was interrupted, the PM might send no response or a response to a 
  // previous command.
  uint8_t SendCommand(uint8_t *cmd, 
                      uint8_t  cmdLen);

  uint8_t GetResponse(uint8_t  *rsp, 
                      uint16_t *rspLen);

  //-----------------------------------------------------------------------------------
  // USBDeviceConfig::Init implementation
  //   Called by USB class when in its Configuring method, which happens automatically
  //   when a device or hub is plugged in or removed (or this USB host turns on).
  virtual uint8_t Init(uint8_t parent, 
                       uint8_t port, 
                       bool    lowspeed);

  //-----------------------------------------------------------------------------------
  // USBDeviceConfig::Release implementation
  //   Automatically called by USB class when this device is unplugged
  virtual uint8_t Release();

  //-----------------------------------------------------------------------------------
  // USBDeviceConfig::Poll implementation
  //   Automatically called by USB class in its Task() method - the USB class makes no
  //   attempt to limit the rate so any interframe gaps or other timing requirements
  //   must be implemented by this code.
  virtual uint8_t Poll();

  //-----------------------------------------------------------------------------------
  // USBDeviceConfig::GetAddress implementation
  //   Called by USB class in its ReleaseDevice(uint8_t addr) method, for example when
  //   a hub reports a disconnect
  virtual uint8_t GetAddress();

  //-----------------------------------------------------------------------------------
  // UsbConfigXtracter::EndpointXtract implementation
  //   Called by template ConfigDescParser::ParseDescriptor for a descriptor of type
  //   USB_DESCRIPTOR_ENDPOINT
  virtual void EndpointXtract(uint8_t conf, 
                              uint8_t iface, 
                              uint8_t alt,
                              uint8_t proto, 
                              const USB_ENDPOINT_DESCRIPTOR *ep);
};

#endif

