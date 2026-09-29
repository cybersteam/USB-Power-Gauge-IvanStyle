#include "fmt.h"

size_t fmt_u32(char *dst, size_t cap, uint32_t value) {
    char tmp[10];
    uint8_t n = 0;
    uint8_t i;

    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    if (cap < (size_t)n + 1u) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        dst[i] = tmp[(uint8_t)(n - 1u - i)];
    }
    dst[n] = '\0';
    return n;
}

size_t fmt_i32(char *dst, size_t cap, int32_t value) {
    uint32_t mag;
    size_t n;

    if (value >= 0) {
        return fmt_u32(dst, cap, (uint32_t)value);
    }
    if (cap < 2u) {
        return 0;
    }
    dst[0] = '-';
    mag = (value == (int32_t)(-2147483647 - 1)) ? 2147483648u : (uint32_t)(-value);
    n = fmt_u32(dst + 1, cap - 1u, mag);
    return n == 0u ? 0u : n + 1u;
}
