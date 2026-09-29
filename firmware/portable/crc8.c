#include "crc8.h"

uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        uint8_t bit;
        crc = (uint8_t)(crc ^ data[i]);
        for (bit = 0; bit < 8u; bit++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}
