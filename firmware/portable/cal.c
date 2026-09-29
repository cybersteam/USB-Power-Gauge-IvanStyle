#include "cal.h"

#include "config.h"
#include "crc8.h"

/*
 * EEPROM record, little-endian, 16 bytes at address 0:
 *   0  'U'
 *   1  'P'
 *   2  version (1)
 *   3  OSCCAL override, 0 = leave the chip default
 *   4  vbg_mV          uint16
 *   6  offset_mA       int16
 *   8  gain_q12        uint16   4096 = 1.000
 *  10  crc8 of bytes 0..9
 *  11  reserved, written 0
 *
 * A blank chip (0xFF) or a failed CRC falls through to the original Adafruit
 * record: a big-endian bandgap in millivolts at bytes 0..1, accepted only
 * inside the 800..1300 window that sketch used.
 */

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_i16(uint8_t *p, int16_t value) {
    put_u16(p, (uint16_t)value);
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t get_i16(const uint8_t *p) {
    return (int16_t)get_u16(p);
}

static int ranges_ok(uint16_t vbg, int16_t offset, uint16_t gain) {
    if (vbg < CAL_VBG_MIN || vbg > CAL_VBG_MAX) {
        return 0;
    }
    if (offset < CAL_OFFSET_MIN_MA || offset > CAL_OFFSET_MAX_MA) {
        return 0;
    }
    if (gain < CAL_GAIN_MIN_Q12 || gain > CAL_GAIN_MAX_Q12) {
        return 0;
    }
    return 1;
}

void cal_defaults(cal_t *cal) {
    cal->vbg_mV = VBG_ASSUMED_MV;
    cal->offset_mA = 0;
    cal->gain_q12 = CAL_GAIN_UNITY_Q12;
    cal->osccal = 0;
    cal->source = CAL_DEFAULT;
}

int cal_encode(const cal_t *cal, uint8_t out[CAL_IMAGE_LEN]) {
    uint8_t i;

    if (!ranges_ok(cal->vbg_mV, cal->offset_mA, cal->gain_q12)) {
        return -1;
    }
    for (i = 0; i < CAL_IMAGE_LEN; i++) {
        out[i] = 0;
    }
    out[0] = (uint8_t)'U';
    out[1] = (uint8_t)'P';
    out[2] = 1u;
    out[3] = cal->osccal;
    put_u16(&out[4], cal->vbg_mV);
    put_i16(&out[6], cal->offset_mA);
    put_u16(&out[8], cal->gain_q12);
    out[10] = crc8(out, 10u);
    return 0;
}

void cal_decode(const uint8_t *image, size_t len, cal_t *cal) {
    uint16_t legacy;

    cal_defaults(cal);
    if (image == 0) {
        return;
    }

    if (len >= CAL_IMAGE_LEN && image[0] == (uint8_t)'U' && image[1] == (uint8_t)'P' &&
        image[2] == 1u && crc8(image, 10u) == image[10]) {
        uint16_t vbg = get_u16(&image[4]);
        int16_t offset = get_i16(&image[6]);
        uint16_t gain = get_u16(&image[8]);
        if (ranges_ok(vbg, offset, gain)) {
            cal->vbg_mV = vbg;
            cal->offset_mA = offset;
            cal->gain_q12 = gain;
            cal->osccal = image[3];
            cal->source = CAL_FACTORY;
            return;
        }
    }

    if (len >= 2u) {
        legacy = (uint16_t)(((uint16_t)image[0] << 8) | image[1]);
        if (legacy >= CAL_VBG_MIN && legacy <= CAL_VBG_MAX) {
            cal->vbg_mV = legacy;
            cal->source = CAL_LEGACY;
        }
    }
}

int cal_vbg_from_reference(uint16_t assumed_vbg_mV, uint16_t reported_mV, uint16_t meter_mV,
                           uint16_t *vbg_mV) {
    uint32_t solved;

    if (assumed_vbg_mV == 0u || reported_mV == 0u || meter_mV == 0u || vbg_mV == 0) {
        return -1;
    }
    solved = ((uint32_t)assumed_vbg_mV * (uint32_t)meter_mV + (uint32_t)reported_mV / 2u) /
             (uint32_t)reported_mV;
    if (solved < CAL_VBG_MIN || solved > CAL_VBG_MAX) {
        return -1;
    }
    *vbg_mV = (uint16_t)solved;
    return 0;
}

int cal_gain_from_load(uint16_t reported_mA, uint16_t true_mA, uint16_t *gain_q12) {
    uint32_t gain;

    if (reported_mA == 0u || gain_q12 == 0) {
        return -1;
    }
    gain = ((uint32_t)true_mA * (uint32_t)CAL_GAIN_UNITY_Q12 + (uint32_t)reported_mA / 2u) /
           (uint32_t)reported_mA;
    if (gain < CAL_GAIN_MIN_Q12 || gain > CAL_GAIN_MAX_Q12) {
        return -1;
    }
    *gain_q12 = (uint16_t)gain;
    return 0;
}
