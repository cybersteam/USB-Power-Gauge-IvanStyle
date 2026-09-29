#include "config.h"

#include "adc.h"
#include "cal.h"
#include "charlie.h"
#include "display.h"
#include "fmt.h"
#include "measure.h"
#include "protocol.h"
#include "timebase.h"
#include "uart.h"

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

/*
 * .init sections are concatenated into the reset path. They are not called,
 * so this has to be naked: a ret would skip the rest of crt startup.
 * MCUSR is saved before the watchdog is disarmed. WDRF must be clear or a
 * watchdog reset immediately re-arms the timer and the chip loops.
 */
uint8_t g_reset_flags __attribute__((section(".noinit")));

void early_init(void) __attribute__((naked, used, section(".init3")));
void early_init(void) {
    g_reset_flags = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

static cal_t g_cal;
static meter_t g_meter;
static gauge_t g_gauge;
static reading_t g_reading;
static uint8_t g_have;
static uint8_t g_uncal;

static void emit_ram(const char *s, uint8_t n) {
    uart_write_blocking(s, n);
}

static void emit_p(const char *progmem) {
    char buf[24];
    uint8_t n = 0;
    char c;

    while ((c = (char)pgm_read_byte(progmem++)) != '\0') {
        buf[n++] = c;
        if (n == (uint8_t)sizeof buf) {
            emit_ram(buf, n);
            n = 0;
        }
    }
    if (n != 0u) {
        emit_ram(buf, n);
    }
}

static void emit_u32(uint32_t value) {
    char buf[11];
    size_t n = fmt_u32(buf, sizeof buf, value);
    if (n != 0u) {
        emit_ram(buf, (uint8_t)n);
    }
}

static void emit_i32(int32_t value) {
    char buf[12];
    size_t n = fmt_i32(buf, sizeof buf, value);
    if (n != 0u) {
        emit_ram(buf, (uint8_t)n);
    }
}

static char hex_digit(uint8_t nibble) {
    if (nibble < 10u) {
        return (char)('0' + nibble);
    }
    return (char)('A' + (int)(nibble - 10u));
}

static void emit_reset(uint8_t flags) {
    uint8_t any = 0;

    if (flags & (uint8_t)_BV(WDRF)) {
        emit_p(PSTR("WDT"));
        any = 1u;
    }
    if (flags & (uint8_t)_BV(BORF)) {
        if (any) {
            emit_p(PSTR("+"));
        }
        emit_p(PSTR("BOR"));
        any = 1u;
    }
    if (flags & (uint8_t)_BV(EXTRF)) {
        if (any) {
            emit_p(PSTR("+"));
        }
        emit_p(PSTR("EXT"));
        any = 1u;
    }
    if (flags & (uint8_t)_BV(PORF)) {
        if (any) {
            emit_p(PSTR("+"));
        }
        emit_p(PSTR("POR"));
        any = 1u;
    }
    if (!any) {
        emit_p(PSTR("none"));
    }
}

static void banner(void) {
    uint8_t osc = OSCCAL;

    emit_p(PSTR("# usb-power-gauge " FW_VERSION_STR " hw=attiny85v-10 f_cpu="));
    emit_u32(F_CPU);
    emit_p(PSTR("\r\n"));

#if F_CPU != 8000000UL
    emit_p(PSTR("# warn clock is outside the ATtiny85V-10S 10 MHz rating\r\n"));
#endif

    emit_p(PSTR("# rst="));
    emit_reset(g_reset_flags);
    if (g_cal.source == CAL_FACTORY) {
        emit_p(PSTR(" cal=factory"));
    } else if (g_cal.source == CAL_LEGACY) {
        emit_p(PSTR(" cal=legacy"));
    } else {
        emit_p(PSTR(" cal=default"));
    }
    emit_p(PSTR(" vbg="));
    emit_u32(g_cal.vbg_mV);
    emit_p(PSTR(" off="));
    emit_i32(g_cal.offset_mA);
    emit_p(PSTR(" g="));
    emit_u32(g_cal.gain_q12);
    emit_p(PSTR("\r\n"));

    emit_p(PSTR("# osccal=0x"));
    {
        char hex[2];
        hex[0] = hex_digit((uint8_t)(osc >> 4));
        hex[1] = hex_digit((uint8_t)(osc & 0x0fu));
        emit_ram(hex, 2);
    }
    emit_p(PSTR("\r\n"));
    emit_p(PSTR("# $UPG,1,mV,mA,mW,uWh,fl,iPk,vMin*crc\r\n"));
}

static void measure_once(uint16_t dt_ms) {
    adc_frame_t adc;
    uint16_t vbus = 0;
    uint16_t current = 0;
    int saturated = 0;

    adc_read(&adc);
    if (adc.ok) {
        uint16_t pin = measure_pin_mV(adc.i_sum, adc.samples, g_cal.vbg_mV);
        vbus = measure_vbus_mV(adc.vbus_sum, adc.samples, g_cal.vbg_mV);
        current = measure_current_mA(pin, g_cal.offset_mA, g_cal.gain_q12);
        saturated = adc.i_max >= ADC_SAT_COUNTS;
    }
    meter_apply_units(&g_meter, vbus, current, saturated, dt_ms, g_uncal, adc.ok, &g_reading);
    g_have = 1u;
}

static void render(uint16_t dt_ms, int blink_on, int slow_on) {
    uint8_t bright[GAUGE_LEDS];

    if (!g_have) {
        return;
    }
    gauge_render(&g_gauge, &g_reading, dt_ms, blink_on, slow_on, bright);
    leds_set(bright);
}

static void report(void) {
    char frame[PROTOCOL_FRAME_MAX];
    int n;

    if (!g_have) {
        return;
    }
    n = protocol_format(&g_reading, frame, sizeof frame);
    if (n > 0) {
        (void)uart_write(frame, (uint8_t)n);
    }
}

static void show_lamp(uint8_t step) {
    uint8_t bright[GAUGE_LEDS];
    uint8_t i;

    for (i = 0; i < GAUGE_LEDS; i++) {
        bright[i] = (i == step) ? LAMP_BRIGHT : 0u;
    }
    leds_set(bright);
}

static void load_calibration(void) {
    uint8_t image[CAL_IMAGE_LEN];

    eeprom_read_block(image, (const void *)0, CAL_IMAGE_LEN);
    cal_decode(image, CAL_IMAGE_LEN, &g_cal);
    g_uncal = (uint8_t)(g_cal.source == CAL_DEFAULT);
    if (g_cal.source == CAL_FACTORY && g_cal.osccal != 0u) {
        OSCCAL = g_cal.osccal;
    }
}

int main(void) {
    uint32_t lamp_t0;
    uint32_t last_meas = 0;
    uint32_t last_report = 0;
    uint32_t last_blink = 0;
    uint32_t last_slow = 0;
    uint8_t lamp_step = 0xffu;
    int blink_on = 1;
    int slow_on = 1;
    int running = 0;

    PRR = (uint8_t)_BV(PRUSI);
    load_calibration();
    leds_init();
    adc_init();
    uart_init();
    timebase_init();
    meter_reset(&g_meter);
    gauge_reset(&g_gauge);

    wdt_enable(WDTO_1S);
    sei();
    banner();

    lamp_t0 = time_ms();

    for (;;) {
        uint32_t now = time_ms();
        int blink_edge = 0;
        int slow_edge = 0;

        wdt_reset();

        if (!running) {
            uint8_t step = (uint8_t)((now - lamp_t0) / LAMP_STEP_MS);
            if (step >= GAUGE_LEDS) {
                running = 1;
                last_meas = now;
                last_report = now;
                last_blink = now;
                last_slow = now;
            } else if (step != lamp_step) {
                lamp_step = step;
                show_lamp(step);
            }
            continue;
        }

        if ((now - last_blink) >= BLINK_PERIOD_MS) {
            last_blink = now;
            blink_on = !blink_on;
            blink_edge = 1;
        }
        if ((now - last_slow) >= SLOW_BLINK_MS) {
            last_slow = now;
            slow_on = !slow_on;
            slow_edge = 1;
        }

        if ((now - last_meas) >= MEASURE_PERIOD_MS) {
            uint32_t elapsed = now - last_meas;
            uint16_t dt = 0;
            if (g_have && elapsed <= 65535ul) {
                dt = (uint16_t)elapsed;
            }
            last_meas = now;
            measure_once(dt);
            render(dt, blink_on, slow_on);
            blink_edge = 0;
            slow_edge = 0;
        } else if ((blink_edge || slow_edge) && g_have) {
            render(0, blink_on, slow_on);
        }

        if (g_have && (now - last_report) >= REPORT_PERIOD_MS) {
            last_report = now;
            report();
        }
    }
}
