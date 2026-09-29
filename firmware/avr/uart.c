#include "uart.h"

#include "board.h"
#include "clock.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

/*
 * Single-producer (main), single-consumer (Timer1) ring.
 * One slot stays empty so head == tail means idle.
 * Main publishes `tail` only after the bytes are in the buffer, so the ISR
 * never observes a torn frame and the copy does not need interrupts off.
 * Holding cli() across a 60-byte copy would stretch a bit by most of its width.
 */

#define TX_SIZE 64u
#define TX_MASK (TX_SIZE - 1u)

enum { TX_IDLE = 0, TX_BITS = 1, TX_STOP = 2 };

static volatile uint8_t g_buf[TX_SIZE];
static volatile uint8_t g_head;
static volatile uint8_t g_tail;
static volatile uint8_t g_state;
static uint8_t g_byte;
static uint8_t g_nbits;

_Static_assert((TX_SIZE & (TX_SIZE - 1u)) == 0u, "TX ring must be a power of two");
_Static_assert(TX_SIZE - 1u >= 60u, "TX ring must hold one telemetry frame");

void uart_init(void) {
    DDRB = (uint8_t)(DDRB | (uint8_t)_BV(PIN_TX));
    PORTB = (uint8_t)(PORTB | (uint8_t)_BV(PIN_TX));
    g_head = 0;
    g_tail = 0;
    g_state = TX_IDLE;
    OCR1A = UART_TOP;
    OCR1C = UART_TOP;
    TCCR1 = (uint8_t)(_BV(CTC1) | UART_CS);
    TIMSK = (uint8_t)(TIMSK | (uint8_t)_BV(OCIE1A));
}

int uart_write(const char *data, uint8_t len) {
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    uint8_t free;
    uint8_t cursor;
    uint8_t i;

    if (len == 0u) {
        return 0;
    }

    head = g_head;
    tail = g_tail;
    used = (uint8_t)((tail - head) & TX_MASK);
    free = (uint8_t)(TX_SIZE - 1u - used);
    if (len > free) {
        return -1;
    }

    cursor = tail;
    for (i = 0; i < len; i++) {
        g_buf[cursor] = (uint8_t)data[i];
        cursor = (uint8_t)((cursor + 1u) & TX_MASK);
    }
    g_tail = cursor;
    return 0;
}

void uart_write_blocking(const char *data, uint8_t len) {
    uint8_t off = 0;

    while (off < len) {
        uint8_t chunk = (uint8_t)(len - off);
        wdt_reset();
        if (chunk > 32u) {
            chunk = 32u;
        }
        if (uart_write(data + off, chunk) == 0) {
            off = (uint8_t)(off + chunk);
        }
    }
}

static void tx_low(void) {
    PORTB = (uint8_t)(PORTB & (uint8_t)~_BV(PIN_TX));
}

static void tx_high(void) {
    PORTB = (uint8_t)(PORTB | (uint8_t)_BV(PIN_TX));
}

ISR(TIMER1_COMPA_vect) {
    uint8_t head;

    if (g_state == TX_IDLE) {
        head = g_head;
        if (head == g_tail) {
            return;
        }
        g_byte = g_buf[head];
        g_head = (uint8_t)((head + 1u) & TX_MASK);
        g_nbits = 0;
        g_state = TX_BITS;
        tx_low();
        return;
    }

    if (g_state == TX_BITS) {
        if (g_nbits < 8u) {
            if (g_byte & 1u) {
                tx_high();
            } else {
                tx_low();
            }
            g_byte = (uint8_t)(g_byte >> 1);
            g_nbits++;
            return;
        }
        tx_high();
        g_state = TX_STOP;
        return;
    }

    g_state = TX_IDLE;
}
