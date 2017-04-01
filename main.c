// main.c

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "common.h"


#if 0
static void delay_tenths(uint8_t t) {
  while (t-- > 0)
    _delay_ms(100);
}

static void blink(uint8_t count, uint8_t on_tenths, uint8_t off_tenths) {
  for (; count > 0; count--) {
    set_bit(PORTD, 6); // LED on
    delay_tenths(on_tenths);
    clr_bit(PORTD, 6); // LED off
    delay_tenths(off_tenths);
  }
}
#endif


struct KeyboardReport {
  uint8_t modifier;
  uint8_t reserved;
  uint8_t key[32];
};

static volatile struct KeyboardReport report = {0, 0};


int main(void)
{
  set_bit(DDRD, 6);   // data direction register, PORTD
  clr_bit(PORTD, 6);  // LED off

  usb_init();

  sei();              // set interrupts on

  while (1) {

    if (usb_configured == 0) {

      set_bit(PORTD, 6); // LED on
      _delay_ms(50);
      clr_bit(PORTD, 6); // LED off
      _delay_ms(50);

    } else {

      report.key[1] = 0x08; // 'h' key down
      usb_write(sizeof(report), (uint8_t const *)&report);
      _delay_ms(100);

      report.key[1] = 0x00; // 'h' key up
      usb_write(sizeof(report), (uint8_t const *)&report);
      _delay_ms(1000);

    }
  }
}
