#include "suite.h"

#include "cal.h"
#include "harness.h"

#include <stdio.h>
#include <string.h>

static void expect_default(const cal_t *cal) {
    EXPECT_EQ(cal->source, CAL_DEFAULT);
    EXPECT_EQ(cal->vbg_mV, 1100);
    EXPECT_EQ(cal->offset_mA, 0);
    EXPECT_EQ(cal->gain_q12, 4096);
    EXPECT_EQ(cal->osccal, 0);
}

void test_cal(void) {
    uint8_t image[CAL_IMAGE_LEN];
    uint8_t blank[CAL_IMAGE_LEN];
    cal_t cal;
    cal_t roundtrip;
    uint16_t vbg = 0;
    uint16_t gain = 0;
    size_t i;
    FILE *out;

    for (i = 0; i < CAL_IMAGE_LEN; i++) {
        blank[i] = 0xffu;
    }
    cal_decode(blank, CAL_IMAGE_LEN, &cal);
    expect_default(&cal);

    blank[0] = 0x04;
    blank[1] = 0x4c; /* 1100, the original big-endian layout */
    cal_decode(blank, CAL_IMAGE_LEN, &cal);
    EXPECT_EQ(cal.source, CAL_LEGACY);
    EXPECT_EQ(cal.vbg_mV, 1100);
    EXPECT_EQ(cal.gain_q12, 4096);

    blank[0] = 0x03;
    blank[1] = 0x20; /* 800 */
    cal_decode(blank, 2, &cal);
    EXPECT_EQ(cal.source, CAL_LEGACY);
    EXPECT_EQ(cal.vbg_mV, 800);

    blank[0] = 0x05;
    blank[1] = 0x14; /* 1300 */
    cal_decode(blank, 2, &cal);
    EXPECT_EQ(cal.source, CAL_LEGACY);

    blank[0] = 0x02;
    blank[1] = 0xff; /* 767 */
    cal_decode(blank, 2, &cal);
    expect_default(&cal);

    blank[0] = 0x05;
    blank[1] = 0x15; /* 1301 */
    cal_decode(blank, 2, &cal);
    expect_default(&cal);

    cal_defaults(&cal);
    cal.vbg_mV = 1102;
    cal.offset_mA = -4;
    cal.gain_q12 = 4100;
    cal.osccal = 0x91;
    EXPECT_EQ(cal_encode(&cal, image), 0);
    EXPECT_EQ(image[0], 'U');
    EXPECT_EQ(image[1], 'P');
    EXPECT_EQ(image[2], 1);
    EXPECT_EQ(image[3], 0x91);
    cal_decode(image, CAL_IMAGE_LEN, &roundtrip);
    EXPECT_EQ(roundtrip.source, CAL_FACTORY);
    EXPECT_EQ(roundtrip.vbg_mV, 1102);
    EXPECT_EQ(roundtrip.offset_mA, -4);
    EXPECT_EQ(roundtrip.gain_q12, 4100);
    EXPECT_EQ(roundtrip.osccal, 0x91);

    image[10] ^= 0xffu;
    cal_decode(image, CAL_IMAGE_LEN, &roundtrip);
    expect_default(&roundtrip);

    cal.gain_q12 = 100;
    EXPECT_EQ(cal_encode(&cal, image), -1);

    EXPECT_EQ(cal_vbg_from_reference(1100, 4980, 5012, &vbg), 0);
    EXPECT_EQ(vbg, 1107);
    EXPECT_EQ(cal_vbg_from_reference(1100, 0, 5012, &vbg), -1);
    EXPECT_EQ(cal_gain_from_load(230, 250, &gain), 0);
    EXPECT_EQ(gain, 4452);
    EXPECT_EQ(cal_gain_from_load(0, 250, &gain), -1);

    out = fopen("build/host/sample.eep", "wb");
    EXPECT_TRUE(out != 0);
    if (out != 0) {
        cal_defaults(&cal);
        cal.vbg_mV = 1102;
        cal.offset_mA = -4;
        cal.gain_q12 = 4100;
        cal.osccal = 0x91;
        EXPECT_EQ(cal_encode(&cal, image), 0);
        EXPECT_EQ(fwrite(image, 1, CAL_IMAGE_LEN, out), CAL_IMAGE_LEN);
        fclose(out);
    }
}
