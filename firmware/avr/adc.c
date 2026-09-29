#include "adc.h"

#include "clock.h"
#include "config.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*
 * VBUS is the chip's own supply, measured against the internal bandgap.
 * Current is INA169 OUT on ADC3, measured against that same bandgap, so one
 * stored vbg constant scales both. The ADC clock is 125 kHz, inside the
 * 50..200 kHz window the datasheet requires for full 10-bit accuracy.
 *
 * Conversions run with the CPU awake. ADC noise-reduction sleep stops clk_I/O,
 * which stops Timer1 and would stretch the UART bit that is in progress.
 */

static int convert(uint16_t *sample) {
    uint16_t guard = 40000u;
    uint8_t sreg;
    uint16_t value;

    ADCSRA = (uint8_t)(ADCSRA | (uint8_t)_BV(ADSC));
    while (ADCSRA & (uint8_t)_BV(ADSC)) {
        if (--guard == 0u) {
            return -1;
        }
    }
    sreg = SREG;
    cli();
    value = ADC;
    SREG = sreg;
    *sample = value;
    return 0;
}

static int burst(uint8_t admux, uint16_t *sum, uint16_t *max_sample) {
    uint8_t i;
    uint16_t acc = 0;
    uint16_t peak = 0;

    ADMUX = admux;
    _delay_us(300);

    for (i = 0; i < 2u; i++) {
        uint16_t discard;
        if (convert(&discard) != 0) {
            return -1;
        }
    }

    for (i = 0; i < ADC_SAMPLES; i++) {
        uint16_t sample;
        if (convert(&sample) != 0) {
            return -1;
        }
        acc = (uint16_t)(acc + sample);
        if (sample > peak) {
            peak = sample;
        }
    }

    *sum = acc;
    if (max_sample != 0) {
        *max_sample = peak;
    }
    return 0;
}

void adc_init(void) {
    uint16_t discard;

    DIDR0 = (uint8_t)(DIDR0 | (uint8_t)_BV(ADC3D));
    ADMUX = 0x0Cu; /* bandgap, VCC reference */
    ADCSRA = (uint8_t)(_BV(ADEN) | ADC_PRESCALER);
    _delay_ms(2);
    (void)convert(&discard);
}

void adc_read(adc_frame_t *frame) {
    frame->samples = (uint8_t)ADC_SAMPLES;
    frame->ok = 0;
    frame->vbus_sum = 0;
    frame->i_sum = 0;
    frame->i_max = 0;

    if (burst(0x0Cu, &frame->vbus_sum, 0) != 0) {
        return;
    }
    /* REFS1 selects the 1.1 V bandgap. MUX 3 is ADC3 / PB3. */
    if (burst((uint8_t)(_BV(REFS1) | 3u), &frame->i_sum, &frame->i_max) != 0) {
        return;
    }
    frame->ok = 1u;
}
