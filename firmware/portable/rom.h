#ifndef USB_POWER_GAUGE_ROM_H
#define USB_POWER_GAUGE_ROM_H

/*
 * Const tables must live in flash. avr-gcc copies plain const into RAM.
 * Host builds read the same arrays from ordinary memory.
 */

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define ROM PROGMEM
#define rom_u8(addr) ((uint8_t)pgm_read_byte(addr))
#else
#define ROM
#define rom_u8(addr) (*(const uint8_t *)(addr))
#endif

#endif
