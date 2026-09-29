#include "display.h"

#include "config.h"
#include "gamma.h"

void gauge_reset(gauge_t *gauge) {
    gauge->peak_mW = 0;
    gauge->hold_ms = 0;
}

static uint8_t segment_for(uint16_t mw) {
    if (mw == 0u) {
        return 0;
    }
    if (mw >= OVERLOAD_ON_MW) {
        return 5u;
    }
    return (uint8_t)((mw + 999u) / 1000u);
}

static void track_peak(gauge_t *gauge, uint16_t mw, uint16_t dt_ms) {
    if (mw >= gauge->peak_mW) {
        gauge->peak_mW = mw;
        gauge->hold_ms = PEAK_HOLD_MS;
        return;
    }
    if (gauge->hold_ms > dt_ms) {
        gauge->hold_ms = (uint16_t)(gauge->hold_ms - dt_ms);
        return;
    }
    gauge->hold_ms = 0;
    gauge->peak_mW = mw;
}

void gauge_render(gauge_t *gauge, const reading_t *reading, uint16_t dt_ms, int blink_on,
                  int slow_on, uint8_t bright[GAUGE_LEDS]) {
    uint8_t i;
    uint32_t pw;
    uint8_t seg;

    for (i = 0; i < GAUGE_LEDS; i++) {
        bright[i] = 0;
    }

    if (reading->flags & FLAG_ADC_FAULT) {
        bright[0] = blink_on ? LAMP_BRIGHT : 0u;
        return;
    }

    if (reading->flags & FLAG_VBUS_OK) {
        if (reading->flags & FLAG_UNCALIBRATED) {
            bright[0] = slow_on ? LAMP_BRIGHT : UNCAL_DIM;
        } else {
            bright[0] = LAMP_BRIGHT;
        }
    }

    pw = reading->p_mW;
    if (pw > 65535ul) {
        pw = 65535ul;
    }
    track_peak(gauge, (uint16_t)pw, dt_ms);

    if (reading->flags & FLAG_OVERLOAD) {
        for (i = 1; i <= 4u; i++) {
            bright[i] = LAMP_BRIGHT;
        }
        bright[5] = blink_on ? LAMP_BRIGHT : OVERLOAD_DIM;
        return;
    }

    for (i = 1; i <= 5u; i++) {
        uint16_t hi = (uint16_t)((uint16_t)i * 1000u);
        uint16_t lo = (uint16_t)(((uint16_t)i - 1u) * 1000u);
        if (pw >= hi) {
            bright[i] = LAMP_BRIGHT;
        } else if (pw > lo) {
            uint16_t frac = (uint16_t)(pw - lo);
            uint8_t duty = (uint8_t)((frac * 32u + 500u) / 1000u);
            bright[i] = gamma_apply(duty);
        }
    }

    seg = segment_for(gauge->peak_mW);
    if (seg >= 1u && bright[seg] < PEAK_DIM) {
        bright[seg] = PEAK_DIM;
    }
}
