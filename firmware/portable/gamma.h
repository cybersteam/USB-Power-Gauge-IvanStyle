#ifndef USB_POWER_GAUGE_GAMMA_H
#define USB_POWER_GAUGE_GAMMA_H

#include <stdint.h>

/* Map a linear duty 0..32 onto a perceptual PWM compare value 0..32. */
uint8_t gamma_apply(uint8_t linear);

#endif
