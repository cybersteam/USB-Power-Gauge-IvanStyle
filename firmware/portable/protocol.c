#include "protocol.h"

#include "crc8.h"
#include "fmt.h"

typedef struct {
    char *cursor;
    char *end;
    int overflow;
} wr_t;

static void wr_char(wr_t *wr, char c) {
    if (wr->overflow || wr->cursor + 1 >= wr->end) {
        wr->overflow = 1;
        return;
    }
    *wr->cursor++ = c;
}

static void wr_str(wr_t *wr, const char *s) {
    while (*s != '\0') {
        wr_char(wr, *s++);
    }
}

static void wr_u32(wr_t *wr, uint32_t value) {
    char tmp[11];
    size_t n;
    size_t i;

    n = fmt_u32(tmp, sizeof tmp, value);
    if (n == 0u) {
        wr->overflow = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        wr_char(wr, tmp[i]);
    }
}

static char hex_digit(uint8_t nibble) {
    if (nibble < 10u) {
        return (char)('0' + nibble);
    }
    return (char)('A' + (int)(nibble - 10u));
}

static void wr_hex8(wr_t *wr, uint8_t value) {
    wr_char(wr, hex_digit((uint8_t)(value >> 4)));
    wr_char(wr, hex_digit((uint8_t)(value & 0x0fu)));
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

int protocol_format(const reading_t *reading, char *dst, size_t cap) {
    char body[56];
    wr_t wr;
    size_t body_len;
    uint8_t crc;
    size_t need;

    wr.cursor = body;
    wr.end = body + sizeof body;
    wr.overflow = 0;

    wr_str(&wr, "UPG,");
    wr_u32(&wr, PROTOCOL_VERSION);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->vbus_mV);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->i_mA);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->p_mW);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->energy_uWh);
    wr_char(&wr, ',');
    wr_hex8(&wr, reading->flags);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->i_peak_mA);
    wr_char(&wr, ',');
    wr_u32(&wr, reading->v_min_mV);

    if (wr.overflow) {
        return -1;
    }

    body_len = (size_t)(wr.cursor - body);
    crc = crc8((const uint8_t *)body, body_len);
    need = 1u + body_len + 1u + 2u + 2u + 1u;
    if (cap < need) {
        return -1;
    }

    dst[0] = '$';
    {
        size_t i;
        for (i = 0; i < body_len; i++) {
            dst[1u + i] = body[i];
        }
    }
    dst[1u + body_len] = '*';
    dst[2u + body_len] = hex_digit((uint8_t)(crc >> 4));
    dst[3u + body_len] = hex_digit((uint8_t)(crc & 0x0fu));
    dst[4u + body_len] = '\r';
    dst[5u + body_len] = '\n';
    dst[6u + body_len] = '\0';
    return (int)(body_len + 6u);
}

int protocol_crc_ok(const char *line) {
    const char *dollar;
    const char *star;
    size_t n;
    int hi;
    int lo;
    uint8_t expect;

    if (line == 0) {
        return 0;
    }
    dollar = line;
    while (*dollar != '\0' && *dollar != '$') {
        dollar++;
    }
    if (*dollar != '$') {
        return 0;
    }
    star = dollar + 1;
    while (*star != '\0' && *star != '*') {
        star++;
    }
    if (*star != '*') {
        return 0;
    }
    n = (size_t)(star - (dollar + 1));
    hi = hex_val(star[1]);
    lo = hex_val(star[2]);
    if (hi < 0 || lo < 0) {
        return 0;
    }
    expect = crc8((const uint8_t *)(dollar + 1), n);
    return expect == (uint8_t)((hi << 4) | lo);
}
