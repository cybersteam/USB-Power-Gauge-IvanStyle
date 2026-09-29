#ifndef USB_POWER_GAUGE_CRC8_H
#define USB_POWER_GAUGE_CRC8_H

#include <stddef.h>
#include <stdint.h>

/* CRC-8/SMBUS: poly 0x07, init 0x00, no reflect, xorout 0x00. */
uint8_t crc8(const uint8_t *data, size_t len);

#endif
