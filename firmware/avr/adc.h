#ifndef USB_POWER_GAUGE_ADC_H
#define USB_POWER_GAUGE_ADC_H

#include <stdint.h>

typedef struct {
    uint16_t vbus_sum;
    uint16_t i_sum;
    uint16_t i_max;
    uint8_t samples;
    uint8_t ok;
} adc_frame_t;

void adc_init(void);
void adc_read(adc_frame_t *frame);

#endif
