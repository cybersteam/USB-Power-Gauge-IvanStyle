#ifndef USB_POWER_GAUGE_CONFIG_H
#define USB_POWER_GAUGE_CONFIG_H

/*
 * Product behavior for the ATtiny85V USB power gauge.
 * Thresholds match the original silkscreen (OK at about 4.5 V, one LED per watt)
 * and add hysteresis so the lamps do not chatter on a noisy rail.
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#define FW_VERSION_MAJOR 2
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
#define FW_VERSION_STR "2.0.0"

#define BAUD 9600u

#define ADC_SAMPLES 16u
#define ADC_SAT_COUNTS 1022u

#define VBG_ASSUMED_MV 1100u

#define VBUS_OK_ON_MV 4500u
#define VBUS_OK_OFF_MV 4400u

#define VBUS_SPEC_LO_ON_MV 4750u
#define VBUS_SPEC_HI_ON_MV 5250u
#define VBUS_SPEC_LO_OFF_MV 4700u
#define VBUS_SPEC_HI_OFF_MV 5300u

#define VBUS_LOW_ON_MV 4000u
#define VBUS_LOW_OFF_MV 4100u

#define OVERLOAD_ON_MW 5000u
#define OVERLOAD_OFF_MW 4800u

#define IIR_DIV 4
#define ENERGY_GAP_MS 2000u
#define ENERGY_MW_CAP 20000u

#define MEASURE_PERIOD_MS 100u
#define REPORT_PERIOD_MS 250u
#define BLINK_PERIOD_MS 200u
#define SLOW_BLINK_MS 500u
#define PEAK_HOLD_MS 2000u
#define LAMP_STEP_MS 80u

#define PEAK_DIM 4u
#define OVERLOAD_DIM 8u
#define UNCAL_DIM 8u
#define LAMP_BRIGHT 32u

#endif
