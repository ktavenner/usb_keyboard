// usb.c

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "common.h"
#include "identity.h"


#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))  // 7.9 System clock prescaler
#define LSB(n) ((n) & 255)
#define MSB(n) (((n) >> 8) & 255)
#define WORD(n) LSB(n), MSB(n)

volatile uint8_t usb_configured = 0;

struct string_descriptor {  // USB 9.6.7 String
  uint8_t bLength;          // Size of this descriptor in bytes
  uint8_t bDescriptorType;  // STRING Descriptor Type (3)
  int16_t bString[];        // LANGIDs or UNICODE encoded string
};

struct descriptor_entry {
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
  uint8_t const * addr;
};


static const uint8_t PROGMEM hid_report_descriptor[] = {
  0x05, 0x01,                     /* Usage Page (Generic Desktop),        */
  0x09, 0x06,                     /* Usage (Keyboard),                    */
  0xA1, 0x01,                     /* Collection (Application),            */
  /* modifier byte                                                        */
  0x05, 0x07,                     /*   Usage Page (Keyboard)              */
  0x19, 0xE0,                     /*   Usage Minimum (224),               */
  0x29, 0xEF,                     /*   Usage Maximum (231),               */
  0x15, 0x00,                     /*   Logical Minimum (0),               */
  0x25, 0x01,                     /*   Logical Maximum (1),               */
  0x95, 0x08,                     /*   Report Count                       */
  0x75, 0x01,                     /*   Report Size                        */
  0x81, 0x02,                     /*   Input (Data, Variable, Absolute),  */
  /* reserved byte                                                        */
  0x95, 0x01,                     /*   Report Count                       */
  0x75, 0x08,                     /*   Report Size                        */
  0x81, 0x01,                     /*   Input (Constant)                   */
  /* 256 key bits, or 32 bytes                                            */
  0x19, 0x00,                     /*   Usage Minimum (0)                  */
  0x29, 0xff,                     /*   Usage Minimum (255)                */
  0x15, 0x00,                     /*   Logical Minimum (0),               */
  0x25, 0x01,                     /*   Logical Maximum (1)                */
  0x96, 0x00, 0x01,               /*   Report Count (256)                 */
  0x75, 0x01,                     /*   Report Size (1)                    */
  0x81, 0x02,                     /*   Input (Data, Variable, Absolute)   */
  /*                                                                      */
  0xc0                            /* End Collection                       */
};

// HID interface descriptor, HID 1.11 spec, section 6.2.1
#define hid_interface_descriptor_text \
  9,                              /* bLength                              */\
  0x21,                           /* bDescriptorType = HID (0x21)         */\
  0x11, 0x01,                     /* bcdHID                               */\
  0,                              /* bCountryCode                         */\
  1,                              /* bNumDescriptors                      */\
  0x22,                           /* bDescriptorType = HID Report (0x22)  */\
  LSB(sizeof(hid_report_descriptor)), /* wDescriptorLength                */\
  MSB(sizeof(hid_report_descriptor))  /* wDescriptorLength                */\

static const uint8_t PROGMEM hid_interface_descriptor[] = {
  hid_interface_descriptor_text
};

static const uint8_t PROGMEM device_descriptor[] = {
  18,                             /* bLength                              */
  1,                              /* bDescriptorType = Device (1)         */
  0x00, 0x02,                     /* bcdUSB                               */
  0xff,                           /* bDeviceClass                         */
  0xff,                           /* bDeviceSubClass                      */
  0xff,                           /* bDeviceProtocol                      */
  32,                             /* bMaxPacketSize0                      */
  0xc0, 0x16,                     /* idVendor  - 0x03EB = Atmel           */
  0xdb, 0x27,                     /* idProduct - 0x2015 = HID keyboard    */
  0x00, 0x01,                     /* bcdDevice                            */
  1,                              /* iManufacturer                        */
  2,                              /* idProduct                            */
  3,                              /* iSerialNumber                        */
  1                               /* bNumConfigurations                   */
};

static const uint8_t PROGMEM config_descriptor[] = {
  // configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
  9,                              /* bLength;                             */
  2,                              /* bDescriptorType = Configuration (2)  */
  WORD(9  + 9 + 7),            /* wTotalLength                         */
  1,                              /* bNumInterfaces                       */
  1,                              /* bConfigurationValue                  */
  0,                              /* iConfiguration                       */
  0x80,                           /* bmAttributes - bus powered           */
  25,                             /* bMaxPower                            */

  // interface descriptor, USB 9.6.5 p.267-269 Table 9-12, HID p.67
  9,                              /* bLength                              */
  4,                              /* bDescriptorType = Interface (4)      */
  0,                              /* bInterfaceNumber                     */
  0,                              /* bAlternateSetting                    */
  1,                              /* bNumEndpoints                        */
  0xff,                           /* bInterfaceClass - Human Interface Device */
  0xff,                           /* bInterfaceSubClass - Boot Interface Subclass */
  0xff,                           /* bInterfaceProtocol - Keyboard        */
  0,                              /* iInterface                           */

  // hid_interface_descriptor_text,

  // endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
  7,                              /* bLength                              */
  5,                              /* bDescriptorType = Endpoint (5)       */
  0x81,                           /* bEndpointAddress (IN #1)             */
  0x03,                           /* bmAttributes (data, no sync, interrupt) */
  64, 0,                          /* wMaxPacketSize                       */
  10                              /* bInterval                            */
};

static const struct string_descriptor PROGMEM string0 = {2 + 2*1,      3, {0x0409}};
static const struct string_descriptor PROGMEM string1 = {2 + 2*NMANU,  3, MANU};
static const struct string_descriptor PROGMEM string2 = {2 + 2*NPROD,  3, PROD};
static const struct string_descriptor PROGMEM string3 = {2 + 2*NSERNO, 3, SERNO};

// 0x0XXX=Standard, 0x1XXX=Class, 0x2XXX=Vendor, 0x3XXX=Reserved
// USB Table 9-5 Descriptor Types
const struct descriptor_entry PROGMEM descriptor_list[] = {
  {0x0100, 0x0000, sizeof(device_descriptor),     device_descriptor},           // Standard Device
  {0x0200, 0x0000, sizeof(config_descriptor),     config_descriptor},           // Standard Configuration
  {0x2100, 0x0000, sizeof(hid_interface_descriptor), hid_interface_descriptor}, // HID Interface
  {0x2200, 0x0000, sizeof(hid_report_descriptor), hid_report_descriptor},       // HID Report
  {0x0300, 0x0000, 2 + 2*1,      (const uint8_t *)&string0},                    // Standard String
  {0x0301, 0x0409, 2 + 2*NMANU,  (const uint8_t *)&string1},                    // Standard String
  {0x0302, 0x0409, 2 + 2*NPROD,  (const uint8_t *)&string2},                    // Standard String
  {0x0303, 0x0409, 2 + 2*NSERNO, (const uint8_t *)&string3},                    // Standard String
  {0x0000, 0x0000, 0, 0} // end sentinel
};


static uint8_t const * find_descriptor_entry(uint16_t wValue, uint16_t wIndex, uint8_t * pLength) {
  uint8_t const * p = (uint8_t const *)descriptor_list;
  while (1) {
    uint16_t value  = pgm_read_word(p);  p += 2;
    uint16_t index  = pgm_read_word(p);  p += 2;
    uint16_t length = pgm_read_word(p);  p += 2;
    uint8_t const * addr = (uint8_t const *)pgm_read_word(p);  p += 2;
    if (length == 0)
      return 0;
    if (value == wValue && index == wIndex) {
      *pLength = length;
      return addr;
    }
  }
}


#define EP0_MAXPKTSIZE 32  // Control endpoint maxPacketSize
#define EP1_MAXPKTSIZE 64  // Endpoint 1 maxPacketSize

static const uint8_t PROGMEM endpoints[] = { // EPEN, UECFG0X, UECFG1X
  /* 0 */ 1, 0x00, 0x22, // Control OUT,  32, one bank, alloc
  /* 1 */ 1, 0xC1, 0x32, // Interrupt IN, 64, one bank, alloc
  /* 2 */ 0,
  /* 3 */ 0,
  /* 4 */ 0,
  /* 5 */ 0,
  /* 6 */ 0
};

static void init_endpoints(uint8_t first, uint8_t last)
{
  const uint8_t * p = endpoints;    // 23-2 Endpoint activation flow 
  uint8_t n, v0, v1, v2;

  for (n = 0; n <= 6; n++) {
    v0 = pgm_read_byte(p++);
    if (v0) {
      v1 = pgm_read_byte(p++);
      v2 = pgm_read_byte(p++);
    }
 
    if (n >= first && n <= last) {  // configure endpoint size (32) and parameterization
      UENUM  = n;                   // select the endpoint
      UECONX = v0;                  // activate the endpoint
      if (v0) {
        UECFG0X = v1;               // configure endpoint direction and type
        UECFG1X = v2;               // configure endpoint size (32) and parameterization
      }
    }
  }
}


// USB 5.5.3 Control Transfer Packet Size Constraints
static void write_progmem(uint8_t maxPacketSize, uint16_t wLength, uint16_t len, uint8_t const * buf)
{
  uint8_t packetSize = 0;

  while (len && wLength) {
    loop_until_bit_is_set(UEINTX, TXINI); // wait until ready for IN packet

    UEDATX = pgm_read_byte(buf++);
    len--;
    wLength--;
    packetSize++;

    if (packetSize == maxPacketSize) {
      clr_bit(UEINTX, TXINI);             // send IN packet
      packetSize = 0;
    }
  }

  if (wLength || packetSize) {            // less than requested, or partial packet remains
    loop_until_bit_is_set(UEINTX, TXINI); // wait until ready for IN packet
    clr_bit(UEINTX, TXINI);               // send (possibily zero length) IN packet
  }
}


// USB 5.7.3 Interrupt Transfer Packet Size Constraints
static void write_data(uint8_t maxPacketSize, uint16_t wLength, uint16_t len, uint8_t const * buf)
{
  uint8_t packetSize = 0;

  while (len) {
    loop_until_bit_is_set(UEINTX, RWAL); // wait until ready for IN packet

    UEDATX = *buf++;
    len--;
    wLength--;
    packetSize++;

    if (packetSize == maxPacketSize) {
      clr_bit(UEINTX, TXINI);             // send IN packet
      clr_bit(UEINTX, FIFOCON);             // send IN packet
      packetSize = 0;
    }
  }

  if (wLength || packetSize) {            // less than requested, or partial packet remains
    loop_until_bit_is_set(UEINTX, RWAL); // wait until ready for IN packet
    clr_bit(UEINTX, TXINI);             // send IN packet
    clr_bit(UEINTX, FIFOCON);               // send (possibily zero length) IN packet
  }
}


ISR(USB_GEN_vect)   // general / device interrupt
{
  if (bit_is_set(UDINT, EORSTI)) {  // End of Reset
    clr_bit(UDINT, EORSTI);
    init_endpoints(0, 0);
    set_bit(UEIENX, RXSTPE);  // USB_COM_vect: enable Rx SETUP
    usb_configured = 0;
  }

  // if (bit_is_set(UDINT, SOFI)) {
  // }

  // if (bit_is_set(UDINT, SUSPI)) {
  // }

  // if (bit_is_set(UDINT, WAKEUPI)) {
  // }
}


ISR(USB_COM_vect)   // endpoint interrupt
{
  UENUM = 0;
  if (bit_is_set(UEINTX, RXSTPI)) {   // Received SETUP packet

    // 9.3 USB Device Request
    uint16_t wRequest = UEDATX << 8;  wRequest |= UEDATX;
    uint16_t wValue   = UEDATX;       wValue   |= UEDATX << 8;
    uint16_t wIndex   = UEDATX;       wIndex   |= UEDATX << 8;
    uint16_t wLength  = UEDATX;       wLength  |= UEDATX << 8;

    clr_bit(UEINTX, RXSTPI);              // ack SETUP packet
    // UEINTX = ~((1<<RXSTPI) | (1<<RXOUTI) | (1<<TXINI));

    switch (wRequest & 0x7fff) { // ignore bmRequestType Data transfer direction bit

    case 0x0000: // USB 9.4.5 Get Status
      UEDATX = 0;
      UEDATX = 0;
      clr_bit(UEINTX, TXINI);                 // send IN packet
      loop_until_bit_is_set(UEINTX, RXOUTI);  // wait for OUT retry packet
      clr_bit(UEINTX, RXOUTI);                // ack OUT retry packet
      return;

    case 0x0005: // USB 9.4.6 Set Address     // AT90 23.7 Address Setup - RXSTPI and TXINI are both high
      UDADDR = wValue & 0x7F;                 // set UADD, keeping ADDEN clear
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      loop_until_bit_is_set(UEINTX, TXINI);   // wait for STATUS exit
      set_bit(UDADDR, ADDEN);                 // set ADDEN
      return;

    case 0x0006:   // USB 9.4.3 Get Descriptor, 7.1.1 Get_Descriptor Request
    case 0x0106:
    {
      uint8_t length;
      uint8_t const * addr = find_descriptor_entry(wValue, wIndex, &length);
      if (!addr)
        break;   // descriptor type and index, or LANGID not found
      write_progmem(EP0_MAXPKTSIZE, wLength, length, addr);
      return;
    }

    case 0x0008: // USB 9.4.2 Get Configuration
      UEDATX = usb_configured;
      // clr_bit(UEINTX, TXINI);              // send IN packet
      loop_until_bit_is_set(UEINTX, RXOUTI);  // wait for OUT retry packet
      clr_bit(UEINTX, TXINI);                 // send IN packet
      // clr_bit(UEINTX, RXOUTI);             // ack OUT retry packet
      return;

    case 0x0009: // USB 9.4.7 Set Configuration
      usb_configured = wValue;                // select configuration
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      init_endpoints(1, 6);
      UERST = 0x1E;                           // reset all endpoints except endpoint 0
      UERST = 0;
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      clr_bit(UEINTX, RXOUTI);                // send IN zero length packet
      return;

//  case 0x2101: // HID 7.2.1 Get_Report Request
//    write_data(EP0_MAXPKTSIZE, wLength, sizeof(report), (uint8_t const *)&report);
//    return;

//  case 0x2102: // HID 7.2.3 Get_Idle Request
//    break;

//  case 0x2103: // HID 7.2.5 Get_Protocol Request
//    break;

    case 0x2109: // HID 7.2.2 Set Report
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      return;            

    case 0x210a: // HID 7.2.4 Set Idle
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      return;            

    case 0x2300: // USB 11.24.2.7 Get Port Status
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      clr_bit(UEINTX, RXOUTI);                // send IN zero length packet
      return;            

    case 0x2303: // USB 11.24.2.x Set Port Feature
      clr_bit(UEINTX, TXINI);                 // send IN zero length packet
      clr_bit(UEINTX, RXOUTI);                // send IN zero length packet
      return;            

    } // switch (wRequest)

    set_bit(UECONX, STALLRQ);
    set_bit(UECONX, EPEN);
  }
}


void usb_init(void)
{
  // 22.13 USB Software Operating Modes - Power on the USB interface

  // Power on USB pads regulator
  // UHWCON = UIMOD | UIDE | UVCONE | UVREGE   (22.12.1)
  set_bit(UHWCON, UVREGE);              // USB pad regulator
  set_bit(UHWCON, UIMOD);               // USB device mode

  // Configure PLL interface - enable PLL and wait PLL lock 
  PLLCSR = 0x16;                        // p.49 PLL prescaler & PLLE
  loop_until_bit_is_set(PLLCSR, PLOCK); // wait ~100ms for PLL lock

  // Enable USB interface
  // USBCON = USBE | HOST | FRZCLK | OTGPADE | IDTE | VBUSTE
  set_bit(USBCON, USBE);                // enable USB controller
  clr_bit(USBCON, FRZCLK);              // enable clock inputs
  set_bit(USBCON, OTGPADE);             // enable OTG/VBUS pad

  // Configure USB interface (speed, endpoints, ...)
  // init_endpoints(0, 0);

  // Wait for VBUS
  set_bit(PORTD, 6); // LED on
  loop_until_bit_is_set(USBSTA, VBUS);
  clr_bit(PORTD, 6); // LED off

  // Attach USB device
  clr_bit(UDCON, DETACH);               // clear to reconnect the device


  set_bit(UDIEN, EORSTE);               // enable USB_GEN_vect EORSTI
//set_bit(UDIEN, SOFE);                 // enable USB_GEN_vect SOFI
//set_bit(UDIEN, SUSPE);                // enable USB_GEN_vect SUSPI
//set_bit(UDIEN, WAKEUPE);              // enable USB_GEN_vect WAKEUPI
}


void usb_write(uint16_t len, uint8_t const * buf)
{
    cli();
    UENUM = 1;
    write_data(EP1_MAXPKTSIZE, 0xFFFF, len, buf);
    sei();
}
