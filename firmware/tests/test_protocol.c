#include "suite.h"

#include "harness.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

void test_protocol(void) {
    reading_t reading;
    reading_t biggest;
    char frame[96];
    char mutated[96];
    int n;
    int worst;
    FILE *out;

    memset(&reading, 0, sizeof reading);
    reading.vbus_mV = 5012;
    reading.i_mA = 250;
    reading.p_mW = 1253;
    reading.energy_uWh = 10000;
    reading.i_peak_mA = 400;
    reading.v_min_mV = 4900;
    reading.flags = 0x03;

    n = protocol_format(&reading, frame, sizeof frame);
    EXPECT_TRUE(n > 0);
    EXPECT_EQ(n, (long)strlen(frame));
    EXPECT_PREFIX(frame, "$UPG,1,5012,250,1253,10000,03,400,4900*");
    EXPECT_TRUE(protocol_crc_ok(frame));
    EXPECT_EQ(frame[n - 2], '\r');
    EXPECT_EQ(frame[n - 1], '\n');
    EXPECT_TRUE(strcmp(frame, "$UPG,1,5012,250,1253,10000,03,400,4900*9E\r\n") == 0);

    memcpy(mutated, frame, (size_t)n + 1u);
    mutated[7] = (char)(mutated[7] == '0' ? '1' : '0');
    EXPECT_TRUE(!protocol_crc_ok(mutated));
    EXPECT_TRUE(!protocol_crc_ok("UPG,1*00\r\n"));
    EXPECT_TRUE(!protocol_crc_ok(""));

    biggest.vbus_mV = 65535;
    biggest.i_mA = 65535;
    biggest.p_mW = 4294967295ul;
    biggest.energy_uWh = 4294967295ul;
    biggest.i_peak_mA = 65535;
    biggest.v_min_mV = 65535;
    biggest.flags = 0xff;
    worst = protocol_format(&biggest, frame, sizeof frame);
    EXPECT_EQ(worst, 60);
    EXPECT_TRUE(protocol_crc_ok(frame));
    EXPECT_EQ(protocol_format(&reading, frame, 8), -1);

    out = fopen("build/host/sample.frame", "wb");
    EXPECT_TRUE(out != 0);
    if (out != 0) {
        n = protocol_format(&reading, frame, sizeof frame);
        EXPECT_TRUE(fwrite(frame, 1, (size_t)n, out) == (size_t)n);
        fclose(out);
    }
}
