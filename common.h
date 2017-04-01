// common.h

#define set_bit(sfr, bit) (_SFR_BYTE(sfr) |=  _BV(bit))
#define clr_bit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))

extern volatile uint8_t usb_configured;

extern void usb_init(void);
extern void usb_write(uint16_t len, uint8_t const * buf);
