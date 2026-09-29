#include "measure.h"

#include "config.h"

/*
 * AVR int is 16 bits. Every product that can exceed 65535 is widened to
 * uint32_t before the multiply. Host tests run in 32-bit int and would not
 * catch a missing cast.
 */

uint16_t measure_vbus_mV(uint16_t bandgap_sum, uint8_t samples, uint16_t vbg_mV) {
    uint32_t num;
    uint32_t mv;

    if (samples == 0u || bandgap_sum == 0u || vbg_mV == 0u) {
        return 0;
    }
    num = (uint32_t)vbg_mV * 1024ul * (uint32_t)samples;
    mv = (num + (uint32_t)bandgap_sum / 2u) / (uint32_t)bandgap_sum;
    if (mv > 65535ul) {
        mv = 65535ul;
    }
    return (uint16_t)mv;
}

uint16_t measure_pin_mV(uint16_t adc_sum, uint8_t samples, uint16_t vref_mV) {
    uint32_t den;
    uint32_t num;
    uint32_t mv;

    if (samples == 0u) {
        return 0;
    }
    den = 1024ul * (uint32_t)samples;
    num = (uint32_t)adc_sum * (uint32_t)vref_mV;
    mv = (num + den / 2u) / den;
    if (mv > 65535ul) {
        mv = 65535ul;
    }
    return (uint16_t)mv;
}

uint16_t measure_current_mA(uint16_t pin_mV, int16_t offset_mA, uint16_t gain_q12) {
    int32_t adjusted;
    int32_t scaled;

    /* 1 mV at the INA169 load resistor is 1 mA with Rs = 0.1 ohm and RL = 10 kohm. */
    adjusted = (int32_t)pin_mV - (int32_t)offset_mA;
    if (adjusted < 0) {
        adjusted = 0;
    }
    scaled = (adjusted * (int32_t)gain_q12 + 2048) / 4096;
    if (scaled < 0) {
        scaled = 0;
    }
    if (scaled > 65535) {
        scaled = 65535;
    }
    return (uint16_t)scaled;
}

uint32_t measure_power_mW(uint16_t vbus_mV, uint16_t i_mA) {
    return ((uint32_t)vbus_mV * (uint32_t)i_mA + 500ul) / 1000ul;
}

void meter_reset(meter_t *meter) {
    meter->energy_uWh = 0;
    meter->resid_mW_ms = 0;
    meter->i_peak_mA = 0;
    meter->v_min_mV = 65535u;
    meter->v_filt = 0;
    meter->i_filt = 0;
    meter->vbus_ok = 0;
    meter->vbus_spec = 0;
    meter->vbus_low = 0;
    meter->overload = 0;
    meter->filt_init = 0;
    meter->has_last = 0;
}

static int32_t iir_step(int32_t filtered, int32_t raw) {
    int32_t delta = raw - filtered;
    int32_t step;

    if (delta == 0) {
        return filtered;
    }
    step = delta / IIR_DIV;
    if (step == 0) {
        step = (delta > 0) ? 1 : -1;
    }
    return filtered + step;
}

static void publish(const meter_t *meter, uint16_t v, uint16_t i, uint32_t p, int saturated,
                    int uncalibrated, reading_t *out) {
    uint8_t flags = 0;

    if (meter->vbus_ok) {
        flags = (uint8_t)(flags | FLAG_VBUS_OK);
    }
    if (meter->vbus_spec) {
        flags = (uint8_t)(flags | FLAG_VBUS_IN_SPEC);
    }
    if (saturated) {
        flags = (uint8_t)(flags | FLAG_I_SATURATED);
    }
    if (uncalibrated) {
        flags = (uint8_t)(flags | FLAG_UNCALIBRATED);
    }
    if (meter->overload) {
        flags = (uint8_t)(flags | FLAG_OVERLOAD);
    }
    if (meter->vbus_low) {
        flags = (uint8_t)(flags | FLAG_VBUS_LOW);
    }

    out->vbus_mV = v;
    out->i_mA = i;
    out->p_mW = p;
    out->energy_uWh = meter->energy_uWh;
    out->i_peak_mA = meter->i_peak_mA;
    out->v_min_mV = meter->v_min_mV;
    out->flags = flags;
}

void meter_apply_units(meter_t *meter, uint16_t v_raw_mV, uint16_t i_raw_mA, int saturated,
                       uint16_t dt_ms, int uncalibrated, int adc_ok, reading_t *out) {
    uint16_t v;
    uint16_t i;
    uint32_t p;
    uint32_t chunk;

    if (!adc_ok) {
        if (meter->has_last) {
            *out = meter->last;
        } else {
            out->vbus_mV = 0;
            out->i_mA = 0;
            out->p_mW = 0;
            out->energy_uWh = 0;
            out->i_peak_mA = 0;
            out->v_min_mV = 0;
            out->flags = 0;
        }
        out->flags = (uint8_t)(out->flags | FLAG_ADC_FAULT);
        if (uncalibrated) {
            out->flags = (uint8_t)(out->flags | FLAG_UNCALIBRATED);
        }
        return;
    }

    if (!meter->filt_init) {
        meter->v_filt = (int32_t)v_raw_mV;
        meter->i_filt = (int32_t)i_raw_mA;
        meter->filt_init = 1u;
    } else {
        meter->v_filt = iir_step(meter->v_filt, (int32_t)v_raw_mV);
        if (saturated) {
            meter->i_filt = (int32_t)i_raw_mA;
        } else {
            meter->i_filt = iir_step(meter->i_filt, (int32_t)i_raw_mA);
        }
    }

    if (meter->v_filt < 0) {
        meter->v_filt = 0;
    }
    if (meter->i_filt < 0) {
        meter->i_filt = 0;
    }
    if (meter->v_filt > 65535) {
        meter->v_filt = 65535;
    }
    if (meter->i_filt > 65535) {
        meter->i_filt = 65535;
    }

    v = (uint16_t)meter->v_filt;
    i = (uint16_t)meter->i_filt;
    p = measure_power_mW(v, i);

    if (meter->vbus_ok) {
        if (v < VBUS_OK_OFF_MV) {
            meter->vbus_ok = 0;
        }
    } else if (v >= VBUS_OK_ON_MV) {
        meter->vbus_ok = 1u;
    }

    if (meter->vbus_spec) {
        if (v < VBUS_SPEC_LO_OFF_MV || v > VBUS_SPEC_HI_OFF_MV) {
            meter->vbus_spec = 0;
        }
    } else if (v >= VBUS_SPEC_LO_ON_MV && v <= VBUS_SPEC_HI_ON_MV) {
        meter->vbus_spec = 1u;
    }

    if (meter->vbus_low) {
        if (v >= VBUS_LOW_OFF_MV) {
            meter->vbus_low = 0;
        }
    } else if (v < VBUS_LOW_ON_MV) {
        meter->vbus_low = 1u;
    }

    if (saturated || p >= OVERLOAD_ON_MW) {
        meter->overload = 1u;
    } else if (p < OVERLOAD_OFF_MW) {
        meter->overload = 0;
    }

    if (dt_ms > 0u && dt_ms <= ENERGY_GAP_MS) {
        chunk = p;
        if (chunk > ENERGY_MW_CAP) {
            chunk = ENERGY_MW_CAP;
        }
        meter->resid_mW_ms += chunk * (uint32_t)dt_ms;
        meter->energy_uWh += meter->resid_mW_ms / 3600ul;
        meter->resid_mW_ms %= 3600ul;
    }

    if (i > meter->i_peak_mA) {
        meter->i_peak_mA = i;
    }
    if (v < meter->v_min_mV) {
        meter->v_min_mV = v;
    }

    publish(meter, v, i, p, saturated, uncalibrated, out);
    meter->last = *out;
    meter->has_last = 1u;
}
