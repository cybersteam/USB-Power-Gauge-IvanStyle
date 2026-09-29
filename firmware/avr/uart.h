#ifndef USB_POWER_GAUGE_UART_H
#define USB_POWER_GAUGE_UART_H

#include <stdint.h>

void uart_init(void);

/* Queue bytes. Returns 0, or -1 if the whole block does not fit. Never writes a partial block. */
int uart_write(const char *data, uint8_t len);

/* Blocks until the bytes are queued. Safe only with interrupts enabled. Pets the watchdog. */
void uart_write_blocking(const char *data, uint8_t len);

#endif
