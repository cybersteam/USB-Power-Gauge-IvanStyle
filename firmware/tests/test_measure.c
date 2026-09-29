#include "suite.h"

#include "harness.h"
#include "measure.h"

static void settle(meter_t *meter, uint16_t v, uint16_t i, reading_t *out) {
    int n;
    for (n = 0; n < 80; n++) {
        meter_apply_units(meter, v, i, 0, 100, 0, 1, out);
        if (out->vbus_mV == v && out->i_mA == i) {
            return;
        }
    }
    EXPECT_EQ(out->vbus_mV, v);
    EXPECT_EQ(out->i_mA, i);
}

void test_measure_math(void) {
    EXPECT_EQ(measure_vbus_mV(3600, 16, 1100), 5006);
    EXPECT_EQ(measure_vbus_mV(0, 16, 1100), 0);
    EXPECT_EQ(measure_pin_mV(1488, 16, 1100), 100);
    EXPECT_EQ(measure_current_mA(100, 0, 4096), 100);
    EXPECT_EQ(measure_current_mA(100, 5, 4096), 95);
    EXPECT_EQ(measure_current_mA(100, -5, 4096), 105);
    EXPECT_EQ(measure_current_mA(100, 0, 8192), 200);
    EXPECT_EQ(measure_current_mA(100, 200, 4096), 0);
    EXPECT_EQ(measure_current_mA(100, 0, 4120), 101);
    EXPECT_EQ(measure_power_mW(5012, 250), 1253);
    EXPECT_TRUE(measure_power_mW(65535, 65535) > 4000000ul);
}

void test_meter_filter_energy_and_flags(void) {
    meter_t meter;
    reading_t reading;
    uint32_t energy;

    meter_reset(&meter);
    meter_apply_units(&meter, 1000, 100, 0, 0, 0, 1, &reading);
    EXPECT_EQ(reading.vbus_mV, 1000);
    EXPECT_EQ(reading.i_mA, 100);
    EXPECT_EQ(reading.energy_uWh, 0);

    meter_apply_units(&meter, 2000, 100, 0, 0, 0, 1, &reading);
    EXPECT_EQ(reading.vbus_mV, 1250);

    meter_reset(&meter);
    meter_apply_units(&meter, 1000, 100, 0, 0, 0, 1, &reading);
    meter_apply_units(&meter, 1001, 100, 0, 0, 0, 1, &reading);
    EXPECT_EQ(reading.vbus_mV, 1001);

    meter_reset(&meter);
    meter_apply_units(&meter, 5000, 200, 0, 0, 0, 1, &reading);
    meter_apply_units(&meter, 5000, 200, 0, 2000, 0, 1, &reading);
    EXPECT_EQ(reading.p_mW, 1000);
    EXPECT_EQ(reading.energy_uWh, 555);

    energy = reading.energy_uWh;
    meter_apply_units(&meter, 5000, 200, 0, 2001, 0, 1, &reading);
    EXPECT_EQ(reading.energy_uWh, energy);

    meter_reset(&meter);
    meter_apply_units(&meter, 5000, 200, 0, 0, 0, 1, &reading);
    meter_apply_units(&meter, 5000, 200, 0, 1000, 0, 1, &reading);
    EXPECT_EQ(reading.energy_uWh, 277);
    meter_apply_units(&meter, 5000, 200, 0, 1000, 0, 1, &reading);
    EXPECT_EQ(reading.energy_uWh, 555);

    meter_reset(&meter);
    meter_apply_units(&meter, 4499, 0, 0, 0, 0, 1, &reading);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_OK) == 0);
    meter_apply_units(&meter, 4500, 0, 0, 100, 0, 1, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_OK);
    settle(&meter, 4400, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_OK);
    settle(&meter, 4399, 0, &reading);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_OK) == 0);
    settle(&meter, 4499, 0, &reading);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_OK) == 0);
    settle(&meter, 4500, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_OK);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_IN_SPEC) == 0);

    settle(&meter, 5000, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_IN_SPEC);
    settle(&meter, 5251, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_IN_SPEC);
    settle(&meter, 5301, 0, &reading);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_IN_SPEC) == 0);

    meter_reset(&meter);
    settle(&meter, 3999, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_LOW);
    settle(&meter, 4099, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_VBUS_LOW);
    settle(&meter, 4100, 0, &reading);
    EXPECT_TRUE((reading.flags & FLAG_VBUS_LOW) == 0);

    meter_reset(&meter);
    settle(&meter, 5000, 1000, &reading);
    EXPECT_TRUE(reading.flags & FLAG_OVERLOAD);
    EXPECT_EQ(reading.p_mW, 5000);
    settle(&meter, 5000, 970, &reading);
    EXPECT_TRUE(reading.flags & FLAG_OVERLOAD);
    settle(&meter, 5000, 900, &reading);
    EXPECT_TRUE((reading.flags & FLAG_OVERLOAD) == 0);

    meter_reset(&meter);
    meter_apply_units(&meter, 5000, 100, 1, 0, 0, 1, &reading);
    EXPECT_TRUE(reading.flags & FLAG_I_SATURATED);
    EXPECT_TRUE(reading.flags & FLAG_OVERLOAD);
    EXPECT_EQ(reading.i_mA, 100);
    meter_apply_units(&meter, 5000, 300, 1, 100, 0, 1, &reading);
    EXPECT_EQ(reading.i_mA, 300);

    meter_reset(&meter);
    meter_apply_units(&meter, 5000, 100, 0, 0, 0, 1, &reading);
    meter_apply_units(&meter, 5000, 300, 0, 100, 0, 1, &reading);
    EXPECT_EQ(reading.i_mA, 150);

    meter_reset(&meter);
    settle(&meter, 4800, 300, &reading);
    EXPECT_EQ(reading.i_peak_mA, 300);
    EXPECT_EQ(reading.v_min_mV, 4800);
    settle(&meter, 5100, 50, &reading);
    EXPECT_EQ(reading.i_peak_mA, 300);
    EXPECT_EQ(reading.v_min_mV, 4800);

    meter_reset(&meter);
    meter_apply_units(&meter, 5000, 10, 0, 0, 1, 1, &reading);
    EXPECT_TRUE(reading.flags & FLAG_UNCALIBRATED);
    energy = reading.energy_uWh;
    meter_apply_units(&meter, 0, 0, 0, 100, 1, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_ADC_FAULT);
    EXPECT_EQ(reading.vbus_mV, 5000);
    EXPECT_EQ(reading.energy_uWh, energy);
    meter_apply_units(&meter, 5000, 10, 0, 100, 1, 1, &reading);
    EXPECT_TRUE((reading.flags & FLAG_ADC_FAULT) == 0);

    meter_reset(&meter);
    meter_apply_units(&meter, 0, 0, 0, 0, 0, 0, &reading);
    EXPECT_TRUE(reading.flags & FLAG_ADC_FAULT);
    EXPECT_EQ(reading.vbus_mV, 0);
}
