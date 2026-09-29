#include "timebase.h"

#include "charlie.h"
#include "clock.h"

#include <avr/interrupt.h>
#include <avr/io.h>

static volatile uint32_t g_ms;
static volatile uint8_t g_sub;

void timebase_init(void) {
    TCCR0A = (uint8_t)_BV(WGM01);
    TCCR0B = (uint8_t)_BV(CS01);
    OCR0A = (uint8_t)TICK_OCR0A;
    TIMSK |= (uint8_t)_BV(OCIE0A);
}

uint32_t time_ms(void) {
    uint32_t ms;
    uint8_t sreg = SREG;
    cli();
    ms = g_ms;
    SREG = sreg;
    return ms;
}

ISR(TIMER0_COMPA_vect) {
    leds_on_tick();
    if (++g_sub >= TICKS_PER_MS) {
        g_sub = 0;
        g_ms++;
    }
}
