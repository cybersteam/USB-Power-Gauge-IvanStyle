#ifndef USB_POWER_GAUGE_CAL_H
#define USB_POWER_GAUGE_CAL_H

#include <stddef.h>
#include <stdint.h>

#define CAL_IMAGE_LEN 16u
#define CAL_VBG_MIN 800u
#define CAL_VBG_MAX 1300u
#define CAL_OFFSET_MIN_MA (-500)
#define CAL_OFFSET_MAX_MA 500
#define CAL_GAIN_MIN_Q12 2048u
#define CAL_GAIN_MAX_Q12 8192u
#define CAL_GAIN_UNITY_Q12 4096u

enum {
    CAL_DEFAULT = 0,
    CAL_LEGACY = 1,
    CAL_FACTORY = 2
};

typedef struct {
    uint16_t vbg_mV;
    int16_t offset_mA;
    uint16_t gain_q12;
    uint8_t osccal; /* 0 keeps the factory OSCCAL loaded at reset */
    uint8_t source;
} cal_t;

void cal_defaults(cal_t *cal);
int cal_encode(const cal_t *cal, uint8_t out[CAL_IMAGE_LEN]);
void cal_decode(const uint8_t *image, size_t len, cal_t *cal);

int cal_vbg_from_reference(uint16_t assumed_vbg_mV, uint16_t reported_mV, uint16_t meter_mV,
                           uint16_t *vbg_mV);
int cal_gain_from_load(uint16_t reported_mA, uint16_t true_mA, uint16_t *gain_q12);

#endif
