#include "suite.h"

#include "display.h"
#include "gamma.h"
#include "harness.h"

static reading_t reading_of(uint32_t mw, uint8_t flags) {
    reading_t reading;
    reading.vbus_mV = 5000;
    reading.i_mA = 0;
    reading.p_mW = mw;
    reading.energy_uWh = 0;
    reading.i_peak_mA = 0;
    reading.v_min_mV = 5000;
    reading.flags = flags;
    return reading;
}

void test_gamma(void) {
    uint8_t i;
    uint8_t prev = 0;
    EXPECT_EQ(gamma_apply(0), 0);
    EXPECT_EQ(gamma_apply(31), 32);
    EXPECT_EQ(gamma_apply(32), 32);
    EXPECT_EQ(gamma_apply(255), 32);
    for (i = 0; i < 32u; i++) {
        uint8_t v = gamma_apply(i);
        EXPECT_TRUE(v >= prev);
        EXPECT_TRUE(v <= 32u);
        prev = v;
    }
}

void test_display(void) {
    gauge_t gauge;
    uint8_t bright[6];
    reading_t reading;

    gauge_reset(&gauge);
    reading = reading_of(0, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[0], 32);
    EXPECT_EQ(bright[1], 0);
    EXPECT_EQ(bright[5], 0);

    reading = reading_of(0, 0);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[0], 0);

    reading = reading_of(0, FLAG_VBUS_OK | FLAG_UNCALIBRATED);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[0], 32);
    gauge_render(&gauge, &reading, 0, 1, 0, bright);
    EXPECT_EQ(bright[0], 8);

    reading = reading_of(0, FLAG_UNCALIBRATED);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[0], 0);

    gauge_reset(&gauge);
    reading = reading_of(2500, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[1], 32);
    EXPECT_EQ(bright[2], 32);
    EXPECT_EQ(gamma_apply(16), 9);
    EXPECT_EQ(bright[3], 9);
    EXPECT_EQ(bright[4], 0);
    EXPECT_EQ(bright[5], 0);

    reading = reading_of(400, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 100, 1, 1, bright);
    EXPECT_EQ(bright[3], 4);
    EXPECT_TRUE(bright[1] > 0);

    reading = reading_of(400, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 2000, 1, 1, bright);
    EXPECT_EQ(bright[3], 0);

    gauge_reset(&gauge);
    reading = reading_of(2000, FLAG_OVERLOAD);
    gauge_render(&gauge, &reading, 0, 0, 1, bright);
    EXPECT_EQ(bright[1], 32);
    EXPECT_EQ(bright[4], 32);
    EXPECT_EQ(bright[5], 8);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[5], 32);

    gauge_reset(&gauge);
    reading = reading_of(3000, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    reading = reading_of(0, FLAG_ADC_FAULT);
    gauge_render(&gauge, &reading, 50, 1, 1, bright);
    EXPECT_EQ(bright[0], 32);
    EXPECT_EQ(bright[1], 0);
    EXPECT_EQ(bright[3], 0);
    gauge_render(&gauge, &reading, 0, 0, 1, bright);
    EXPECT_EQ(bright[0], 0);
    reading = reading_of(0, FLAG_VBUS_OK);
    gauge_render(&gauge, &reading, 0, 1, 1, bright);
    EXPECT_EQ(bright[3], 4);
}
