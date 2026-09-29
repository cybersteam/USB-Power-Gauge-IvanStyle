#include "harness.h"
#include "suite.h"

#include <stdio.h>

int main(void) {
    test_crc();
    test_fmt();
    test_measure_math();
    test_meter_filter_energy_and_flags();
    test_gamma();
    test_display();
    test_cal();
    test_protocol();

    if (g_failed != 0) {
        fprintf(stderr, "%d failed, %d passed\n", g_failed, g_passed);
        return 1;
    }
    printf("%d passed\n", g_passed);
    return 0;
}
