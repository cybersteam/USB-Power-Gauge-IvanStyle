#ifndef USB_POWER_GAUGE_FMT_H
#define USB_POWER_GAUGE_FMT_H

#include <stddef.h>
#include <stdint.h>

/* Decimal formatters. No printf: the AVR libc formatter costs more flash than this firmware's core. */
size_t fmt_u32(char *dst, size_t cap, uint32_t value);
size_t fmt_i32(char *dst, size_t cap, int32_t value);

#endif
