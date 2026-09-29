#ifndef USB_POWER_GAUGE_PROTOCOL_H
#define USB_POWER_GAUGE_PROTOCOL_H

#include <stddef.h>

#include "measure.h"

#define PROTOCOL_VERSION 1u

/* '$' + body + '*' + CRC + CRLF + NUL. The longest legal frame is 60 bytes. */
#define PROTOCOL_FRAME_MAX 64u

/*
 * Write one telemetry sentence, including CRLF and a terminating NUL.
 * Returns the character count excluding NUL, or -1 if cap is too small.
 *
 *   $UPG,<proto>,<mV>,<mA>,<mW>,<uWh>,<flags>,<iPeak>,<vMin>*<CRC>\r\n
 *
 * CRC-8/SMBUS covers the bytes after '$' and before '*'. flags is two
 * uppercase hex digits. When FLAG_ADC_FAULT is set, discard the sample.
 */
int protocol_format(const reading_t *reading, char *dst, size_t cap);
int protocol_crc_ok(const char *line);

#endif
