#include "PM_usb.h"

// This are the indexes where we store our Interrupt Endpoints
const uint8_t PM_usb::epInterruptInIndex  = 1;	
const uint8_t PM_usb::epInterruptOutIndex = 2;

PM_usb::PM_usb(USB *pusb) : pUsb(pusb) 
{
  Initialize();
  pUsb->RegisterDeviceClass(this);
};

void PM_usb::Initialize()
{
  bPollEnable = false;
  bAddress  = 0;
  pPmPoller = NULL;

  hidInterface.bmInterface = 0;
  hidInterface.bmAltSet    = 0;
  hidInterface.bmProtocol  = 0;

  for (uint8_t i=0; i<MAX_ENDPOINTS; i++)
  {
    epInfo[i].epAddr     = 0;
    epInfo[i].maxPktSize = (i) ? 0 : 8;
    epInfo[i].epAttribs  = 0;
    if (!i) epInfo[i].bmNakPower = USB_NAK_MAX_POWER;
  } 
  bNumEP    = 1; // '0' endpoint is already there and doesn't get configured
                 // during bus enumeration
  bConfNum  = 0;
}

//---------------------------------------------------------------------------------------
// The registered PM_poller is expected to call these methods to send a command and
// get a response.  Typically this happens in the PM_poller's Poll() method.
uint8_t PM_usb::SendCommand(uint8_t *cmd, 
                            uint8_t  cmdLen)
{
  uint8_t epAddr = epInfo[epInterruptOutIndex].epAddr;
  uint8_t rcode = pUsb->outTransfer(bAddress, epAddr, cmdLen, cmd);
    
  if (rcode)
  {
    Notify(PSTR("SendCommand outTransfer error, rcode "));
    Serial.println(rcode, HEX);
  }

  return rcode;
}

uint8_t PM_usb::GetResponse(uint8_t  *rsp, 
                            uint16_t *rspLen)
{
  uint8_t  epAddr = epInfo[epInterruptInIndex].epAddr;
  uint8_t  rcode = pUsb->inTransfer(bAddress, epAddr, rspLen, rsp);

  if (rcode)
  {
    Notify(PSTR("GetResponse inTransfer error, rcode "));
    Serial.println(rcode, HEX);
  } 

  return rcode;
}

//--------------------------------------------------------------------------------------
// PM_usb::Init
//   Called by the USB object when any USB device gets plugged in, or when this 
//   USB host turns on.  This function returns USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED
//   if this class does not handle that particular device.  Otherwise, this function
//   initializes this object with USB comms parameters, registers the device with the
//   USB object, and returns 0.
//
//   Most of this function was copied from UsbUniversal.cpp
//
uint8_t PM_usb::Init(uint8_t parent, 
                     uint8_t port, 
                     bool    lowspeed) {
  Notify(PSTR(">PM_usb::Init parent ")); Serial.print(parent, HEX);
  Notify(PSTR(", port ")); Serial.print(port, HEX);
  Notify(PSTR(", lowspeed ")); Serial.println(lowspeed, HEX);
  
  const uint8_t constBufSize = sizeof(USB_DEVICE_DESCRIPTOR);

  uint8_t   buf[constBufSize];
  uint8_t   rcode;
  UsbDevice *p = NULL;
  EpInfo    *oldep_ptr = NULL;
  uint8_t   len = 0;
  uint16_t  cd_len = 0;

  uint8_t   num_of_conf;
  uint8_t   num_of_intf;

  AddressPool &addrPool = pUsb->GetAddressPool();

  // We would already be assigned an address only if the USB class 
  // somehow messed up and called our Init again
  if (bAddress) return USB_ERROR_CLASS_INSTANCE_ALREADY_IN_USE;

  // We are going to borrow the USB object's device 0
  p = addrPool.GetUsbDevicePtr(0);
  if (!p) return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;
  if (!p->epinfo) return USB_ERROR_EPINFO_IS_NULL;

  // Save old pointer to EP_RECORD of address 0
  oldep_ptr = p->epinfo;

  // Temporary assign new pointer to epInfo to p->epinfo in order to avoid toggle inconsistence
  p->epinfo = epInfo;

  p->lowspeed = lowspeed;

  // Get device descriptor from the device
  // RLB shouldn't buf be the device descriptor????
  rcode = pUsb->getDevDescr(0,   // addr
                            0,   // ep
                            8,   // nbytes (really, maxbytes), should use size of buf?
                           (uint8_t*)buf);
  // Restore p->epinfo
  p->epinfo = oldep_ptr;

  if (rcode) 
  {
    goto FailGetDevDescr;
  }
  len = (buf[0] > constBufSize) ? constBufSize : buf[0];

  // Allocate new address according to device class
  bAddress = addrPool.AllocAddress(parent, false, port);

  if (!bAddress)
    return USB_ERROR_OUT_OF_ADDRESS_SPACE_IN_POOL;

  // Extract Max Packet Size from the device descriptor
  epInfo[0].maxPktSize = (uint8_t)((USB_DEVICE_DESCRIPTOR*)buf)->bMaxPacketSize0; 

  // Assign new address to the device (i.e., tell the device its address)
  rcode = pUsb->setAddr(0, 0, bAddress);

  if (rcode)
  {
    p->lowspeed = false;
    addrPool.FreeAddress(bAddress);
    bAddress = 0;
    USBTRACE2("setAddr:",rcode);
    return rcode;
  }

  Notify(PSTR(" PM_usb::Init bAddress ")); 
  Serial.println(bAddress, HEX);

  // Not sure what this is for
  p->lowspeed = false;

  p = addrPool.GetUsbDevicePtr(bAddress);

  if (!p) return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

  p->lowspeed = lowspeed;

  if (len)
    rcode = pUsb->getDevDescr(bAddress, 0, len, (uint8_t*)buf);

  if(rcode) goto FailGetDevDescr;

  num_of_conf = ((USB_DEVICE_DESCRIPTOR*)buf)->bNumConfigurations;

  // Assign epInfo to epinfo pointer
  rcode = pUsb->setEpInfoEntry(bAddress, 1 /* count */, epInfo);

  if (rcode) goto FailSetDevTblEntry;

  Notify(PSTR(" PM_usb::Init num_of_conf ")); 
  Serial.println(num_of_conf, DEC);

  for (uint8_t i=0; i<num_of_conf; i++)
  {
    //HexDumper<USBReadParser, uint16_t, uint16_t> HexDump;
    //rcode = pUsb->getConfDescr(bAddress, 0, i, &HexDump);

    ConfigDescParser<USB_CLASS_HID, 0, 0,CP_MASK_COMPARE_CLASS> confDescrParser(this);
    // This call invokes our EndpointExtract for each endpoint descriptor
    // it encounters.  That is where bNumEP gets updated.
    rcode = pUsb->getConfDescr(bAddress, 0, i, &confDescrParser);

    if (bNumEP > 1)
      break;
  }

  if (bNumEP < 2)
  {
    Notify(PSTR("<PM_usb::Init bNUmEP ")); Serial.print(bNumEP, DEC); 
    Notify(PSTR(" < 2 so return DEVICE_NOT_SUPPORTED"));
    return USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;
  }

  // Register this endpoint with the USB class
  rcode = pUsb->setEpInfoEntry(bAddress, bNumEP, epInfo);
  
  if (rcode) {
    Notify(PSTR(" PM_usb::Init failed to setEpInfoEntry, rcode ")); Serial.print(rcode, HEX);
  }

  // Set Configuration Value into the device itself???
  Notify(PSTR(" PM_usb;:Init bConfNum ")); Serial.println(bConfNum, HEX);
  rcode = pUsb->setConf(bAddress, 0 /* ep */, bConfNum);

  if (rcode) goto FailSetConfDescr;

  // Typically the report descriptors would be processed next, but we can ignore them
  // for the PM because they don't contain useful information.

  // Device is ready, so we can poll it now  
  bPollEnable = true;
  
  Notify(PSTR("<PM_usb::Init\r\n"));
  return 0;

FailGetDevDescr:
  USBTRACE("getDevDescr:");
  goto Fail;

FailSetDevTblEntry:
  USBTRACE("setDevTblEn:");
  goto Fail;

FailGetConfDescr:
  USBTRACE("getConf:");
  goto Fail;

FailSetConfDescr:
  USBTRACE("setConf:");
  goto Fail;

FailSetProtocol:
  USBTRACE("SetProto:");
  goto Fail;

FailSetIdle:
  USBTRACE("SetIdle:");
  goto Fail;

FailGetReportDescr:
  USBTRACE("GetReportDescr:");
  goto Fail;

Fail:
  Serial.println(rcode, HEX);
  Release();
  return rcode;
}

//--------------------------------------------------------------------------------------
// Automatically called by USB class when this device is unplugged
uint8_t PM_usb::Release() 
{
  Notify(PSTR("PM_usb::Release\r\n"));
  pUsb->GetAddressPool().FreeAddress(bAddress);
  bNumEP      = 1;
  bAddress    = 0;
  bPollEnable = false;
  if (pPmPoller) pPmPoller->Poll(bPollEnable);
  return 0;
}

//--------------------------------------------------------------------------------------
// Automatically called by USB class in its Task() method - the USB class makes no
// attempt to limit the rate so any interframe gaps or other timing requirements
// must be implemented by this code.
uint8_t PM_usb::Poll() 
{
  if (pPmPoller) pPmPoller->Poll(bPollEnable);
  return 0;
}

//--------------------------------------------------------------------------------------
// Called by USB class in its ReleaseDevice(uint8_t addr) method, for example when
// a hub reports a disconnect
uint8_t PM_usb::GetAddress()
{
  return bAddress;
};

//----------------------------------------------------------------------------
// PM_usb::EndpointXtract
//    Save the endpoint info in our structures
void PM_usb::EndpointXtract(uint8_t conf, 
                            uint8_t iface, 
                            uint8_t alt,
                            uint8_t proto, 
                            const USB_ENDPOINT_DESCRIPTOR *pep)
{
  Notify(PSTR(" >PM_usb::EndpointXtract conf ")); Serial.print(conf, DEC);
  Notify(PSTR(", iface ")); Serial.print(iface, DEC);
  Notify(PSTR(", alt ")); Serial.print(alt, HEX);
  Notify(PSTR(", proto ")); Serial.print(proto, HEX);
  Notify(PSTR(", bNumEP ")); Serial.println(bNumEP, DEC);

  // If the first configuration satisfies, the others are not considered.
  if (bNumEP > 1 && conf != bConfNum)
    return;

  bConfNum = conf;

  uint8_t       index = 0;
  HIDInterface *piface = &hidInterface;

  piface->bmInterface = iface;
  piface->bmAltSet    = alt;
  piface->bmProtocol  = proto;

  if ((pep->bmAttributes & 0x03) == 3 && (pep->bEndpointAddress & 0x80) == 0x80)
  {
    index = epInterruptInIndex;
    Notify(PSTR("  PM_usb::EndpointXtract Interrupt In endpoint, index ")); Serial.println(index, DEC);
  }
  else
  {
    index = epInterruptOutIndex;
    Notify(PSTR("  PM_usb::EndpointXtract Interrupt Out endpoint, index ")); Serial.println(index, DEC);
  }

  if (index)
  {
    // Fill in the endpoint info structure
    epInfo[bNumEP].epAddr     = (pep->bEndpointAddress & 0x0F);
    epInfo[bNumEP].maxPktSize = (uint8_t)pep->wMaxPacketSize;
    epInfo[bNumEP].epAttribs  = 0;

    Notify(PSTR("  PM_usb::EndpointXtract epAddr ")); Serial.print(epInfo[bNumEP].epAddr, HEX);
    Notify(PSTR(", maxPktSize ")); Serial.print(epInfo[bNumEP].maxPktSize, HEX);
    Notify(PSTR(", epAttribs ")); Serial.println(epInfo[bNumEP].epAttribs, HEX);

    bNumEP++;
  }
  //PrintEndpointDescriptor(pep);
  Notify(PSTR(" <PM_usb::EndpointXtract bNumEP ")); Serial.println(bNumEP, DEC);
}


