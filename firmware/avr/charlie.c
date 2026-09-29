#include "charlie.h"

#include "board.h"

#include <avr/pgmspace.h>

/*
 * Drive pairs copied from the original charlieplex map so the silkscreen
 * (OK, 1W..5W) stays wired the way the PCB was built.
 * Index 0 is the green OK lamp. Indexes 1..5 are the watt bar.
 *
 * The ISR releases the three lamp pins before driving the next pair.
 * A direct swap would overlap two outputs and shunt a LED pair.
 */

typedef struct {
    uint8_t ddr;
    uint8_t port;
} led_drive_t;

static const led_drive_t k_drive[6] PROGMEM = {
    {(uint8_t)(_BV(PIN_LED_B) | _BV(PIN_LED_C)), (uint8_t)_BV(PIN_LED_C)},
    {(uint8_t)(_BV(PIN_LED_B) | _BV(PIN_LED_C)), (uint8_t)_BV(PIN_LED_B)},
    {(uint8_t)(_BV(PIN_LED_A) | _BV(PIN_LED_C)), (uint8_t)_BV(PIN_LED_A)},
    {(uint8_t)(_BV(PIN_LED_A) | _BV(PIN_LED_C)), (uint8_t)_BV(PIN_LED_C)},
    {(uint8_t)(_BV(PIN_LED_A) | _BV(PIN_LED_B)), (uint8_t)_BV(PIN_LED_A)},
    {(uint8_t)(_BV(PIN_LED_A) | _BV(PIN_LED_B)), (uint8_t)_BV(PIN_LED_B)},
};

static volatile uint8_t g_bright[2][6];
static volatile uint8_t g_front;
static uint8_t g_slot;
static uint8_t g_pwm;

void leds_init(void) {
    DDRB = (uint8_t)(DDRB & (uint8_t)~LED_PINS);
    PORTB = (uint8_t)(PORTB & (uint8_t)~LED_PINS);
    g_front = 0;
    g_slot = 0;
    g_pwm = 0;
}

void leds_set(const uint8_t bright[6]) {
    uint8_t back = (uint8_t)(g_front ^ 1u);
    uint8_t i;

    for (i = 0; i < 6u; i++) {
        g_bright[back][i] = bright[i];
    }
    __asm__ __volatile__("" ::: "memory");
    g_front = back;
}

void leds_on_tick(void) {
    uint8_t led = g_slot;
    uint8_t level = g_bright[g_front][led];

    DDRB = (uint8_t)(DDRB & (uint8_t)~LED_PINS);
    if (level > g_pwm) {
        uint8_t ddr = pgm_read_byte(&k_drive[led].ddr);
        uint8_t on = pgm_read_byte(&k_drive[led].port);
        PORTB = (uint8_t)((PORTB & (uint8_t)~LED_PINS) | on);
        DDRB = (uint8_t)((DDRB & (uint8_t)~LED_PINS) | ddr);
    } else {
        PORTB = (uint8_t)(PORTB & (uint8_t)~LED_PINS);
    }

    if (++g_slot >= 6u) {
        g_slot = 0;
        g_pwm = (uint8_t)((g_pwm + 1u) & 31u);
    }
}
