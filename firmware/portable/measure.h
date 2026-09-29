#ifndef USB_POWER_GAUGE_MEASURE_H
#define USB_POWER_GAUGE_MEASURE_H

#include <stdint.h>

enum {
    FLAG_VBUS_OK = 1u << 0,
    FLAG_VBUS_IN_SPEC = 1u << 1,
    FLAG_I_SATURATED = 1u << 2,
    FLAG_UNCALIBRATED = 1u << 3,
    FLAG_OVERLOAD = 1u << 4,
    FLAG_VBUS_LOW = 1u << 5,
    FLAG_ADC_FAULT = 1u << 6
};

typedef struct {
    uint16_t vbus_mV;
    uint16_t i_mA;
    uint32_t p_mW;
    uint32_t energy_uWh;
    uint16_t i_peak_mA;
    uint16_t v_min_mV;
    uint8_t flags;
} reading_t;

typedef struct {
    uint32_t energy_uWh;
    uint32_t resid_mW_ms;
    uint16_t i_peak_mA;
    uint16_t v_min_mV;
    int32_t v_filt;
    int32_t i_filt;
    reading_t last;
    uint8_t vbus_ok;
    uint8_t vbus_spec;
    uint8_t vbus_low;
    uint8_t overload;
    uint8_t filt_init;
    uint8_t has_last;
} meter_t;

uint16_t measure_vbus_mV(uint16_t bandgap_sum, uint8_t samples, uint16_t vbg_mV);
uint16_t measure_pin_mV(uint16_t adc_sum, uint8_t samples, uint16_t vref_mV);
uint16_t measure_current_mA(uint16_t pin_mV, int16_t offset_mA, uint16_t gain_q12);
uint32_t measure_power_mW(uint16_t vbus_mV, uint16_t i_mA);

void meter_reset(meter_t *meter);
void meter_apply_units(meter_t *meter, uint16_t v_raw_mV, uint16_t i_raw_mA, int saturated,
                       uint16_t dt_ms, int uncalibrated, int adc_ok, reading_t *out);

#endif
