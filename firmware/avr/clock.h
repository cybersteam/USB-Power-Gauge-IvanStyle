#ifndef USB_POWER_GAUGE_CLOCK_H
#define USB_POWER_GAUGE_CLOCK_H

#include "config.h"

#include <avr/io.h>

/*
 * Timer 0 runs the lamp mux and the millisecond clock.
 * Timer 1 runs the UART bit clock.
 * Both integer ratios below are exact at the two supported clocks.
 * The bit rate is 9615, +0.16% from 9600, before RC-oscillator error.
 */

#if F_CPU == 8000000UL
#define TICK_OCR0A 49u
#define ADC_PRESCALER ((uint8_t)(_BV(ADPS2) | _BV(ADPS1))) /* 8 MHz / 64 = 125 kHz */
#define UART_TOP 103u
#define UART_CS 4u /* Timer1 CK/8 -> 1 MHz, period 104 -> 9615 baud */
#define UART_TIMER_HZ 1000000ul
#elif F_CPU == 16000000UL
#define TICK_OCR0A 99u
#define ADC_PRESCALER ((uint8_t)(_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0))) /* /128 = 125 kHz */
#define UART_TOP 25u
#define UART_CS 7u /* Timer1 CK/64 -> 250 kHz, period 26 -> 9615 baud */
#define UART_TIMER_HZ 250000ul
#else
#error "F_CPU must be 8000000UL (production) or 16000000UL (existing Trinket fuses)"
#endif

#define TICK_HZ 20000ul
#define TICKS_PER_MS 20u

_Static_assert(F_CPU / 8ul / (TICK_OCR0A + 1ul) == TICK_HZ, "lamp tick is not 20 kHz");
_Static_assert(TICK_HZ / 1000ul == TICKS_PER_MS, "millisecond divider");
_Static_assert(UART_TIMER_HZ / (UART_TOP + 1ul) == 9615ul, "uart divider");
_Static_assert((9615ul - BAUD) * 10000ul < BAUD * 50ul, "baud error is 0.5% or more");

#endif
