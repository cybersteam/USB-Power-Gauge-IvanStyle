#ifndef USB_POWER_GAUGE_BOARD_H
#define USB_POWER_GAUGE_BOARD_H

#include <avr/io.h>

/*
 * ATtiny85V-10S pinout on the Adafruit USB Power Gauge PCB.
 * D+ and D- are not connected to the microcontroller.
 *
 *   PB0  charlieplex
 *   PB1  TTL serial TX, idle high, 9600 8N1. Also the ISP MISO pin.
 *   PB2  charlieplex
 *   PB3  INA169 OUT, ADC3, internal 1.1 V reference
 *   PB4  charlieplex
 *   PB5  RESET / ISP
 */

#define PIN_LED_A PB0
#define PIN_TX PB1
#define PIN_LED_B PB2
#define PIN_ISENSE PB3
#define PIN_LED_C PB4

#define LED_PINS ((uint8_t)(_BV(PIN_LED_A) | _BV(PIN_LED_B) | _BV(PIN_LED_C)))

#endif
