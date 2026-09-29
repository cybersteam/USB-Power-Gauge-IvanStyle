#include "gamma.h"

#include "rom.h"

/* round((i / 31)^2 * 32). Endpoints are 0 and 32. The low end stays dark on purpose. */
static const uint8_t k_gamma[32] ROM = {
    0,  0,  0,  0,  1,  1,  1,  2,  2,  3,  3,  4,  5,  6,  7,  7,
    9,  10, 11, 12, 13, 15, 16, 18, 19, 21, 23, 24, 26, 28, 30, 32};

uint8_t gamma_apply(uint8_t linear) {
    if (linear >= 32u) {
        return 32u;
    }
    return rom_u8(&k_gamma[linear]);
}
