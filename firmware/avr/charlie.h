#ifndef USB_POWER_GAUGE_CHARLIE_H
#define USB_POWER_GAUGE_CHARLIE_H

#include <stdint.h>

void leds_init(void);
void leds_set(const uint8_t bright[6]);
void leds_on_tick(void);

#endif
