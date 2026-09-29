#ifndef USB_POWER_GAUGE_DISPLAY_H
#define USB_POWER_GAUGE_DISPLAY_H

#include <stdint.h>

#include "measure.h"

#define GAUGE_LEDS 6u

typedef struct {
    uint16_t peak_mW;
    uint16_t hold_ms;
} gauge_t;

void gauge_reset(gauge_t *gauge);

/*
 * bright[0] is the green OK lamp. bright[1..5] are the 1 W..5 W bar.
 * Values are PWM compares in 0..32 (32 is solid on).
 * dt_ms advances the peak-hold timer. Pass 0 to refresh blink phase only.
 * blink_on is the fast overload / fault phase. slow_on is the uncalibrated phase.
 */
void gauge_render(gauge_t *gauge, const reading_t *reading, uint16_t dt_ms, int blink_on,
                  int slow_on, uint8_t bright[GAUGE_LEDS]);

#endif
