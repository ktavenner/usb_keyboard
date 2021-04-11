// main.c

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "common.h"

struct KeyboardReport {
  uint8_t modifier;
  uint8_t reserved;
  uint8_t key[32];
};

int main(void)
{
  set_bit(DDRD, 6);   // data direction register, PORTD
  clr_bit(PORTD, 6);  // LED off

  usb_init();

  sei();              // set interrupts on

  DDRC = 0xFF;
  DDRD = 0xFF;
  DDRB = 0x00;
  DDRF = 0x00;

  //pullup resistors
  PORTB = 0xFF;
  PORTF = 0xFF;

  //Set output pins on
  PORTC = 0xFF;
  PORTD = 0xFF;

  while (usb_configured == 0) {
    clr_bit(PORTD, 6); // LED off
    _delay_ms(50);
    set_bit(PORTD, 6); // LED on
    _delay_ms(50);
  }

  struct KeyboardReport report = {0, 0};
  uint8_t saved[32];  // saved state of report.key[]
  uint8_t send_report = 1;

  while (1) {
    for (uint8_t i = 0; i < 8; i++) {
      clr_bit(PORTC, i);
      _delay_us(10);
      report.key[i   ] = ~ PINB;
      report.key[i+16] = ~ PINF;
      set_bit(PORTC, i);

      clr_bit(PORTD, i);
      _delay_us(10);
      report.key[i+8 ] = ~ PINB;
      report.key[i+24] = ~ PINF;
      set_bit(PORTD, i);
    }

    for (uint8_t i = 0; i < 32; i++) {
      send_report |= (saved[i] != report.key[i]);
      saved[i] = report.key[i];
    }

    if (send_report) {
      usb_write(sizeof(report), (uint8_t const *)&report);
      send_report = 0;
    }
  }
}
