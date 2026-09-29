#include "suite.h"

#include "crc8.h"
#include "fmt.h"
#include "harness.h"

#include <string.h>

void test_crc(void) {
    const char *check = "123456789";
    EXPECT_EQ(crc8((const uint8_t *)check, 9), 0xF4);
    EXPECT_EQ(crc8((const uint8_t *)"", 0), 0);
    EXPECT_EQ(crc8((const uint8_t *)"A", 1), 0xC0); /* poly step of 0x41 */
}

void test_fmt(void) {
    char buf[16];

    EXPECT_EQ(fmt_u32(buf, sizeof buf, 0), 1);
    EXPECT_TRUE(strcmp(buf, "0") == 0);
    EXPECT_EQ(fmt_u32(buf, sizeof buf, 5012), 4);
    EXPECT_TRUE(strcmp(buf, "5012") == 0);
    EXPECT_EQ(fmt_i32(buf, sizeof buf, -4), 2);
    EXPECT_TRUE(strcmp(buf, "-4") == 0);
    EXPECT_EQ(fmt_u32(buf, 2, 100), 0);
}
