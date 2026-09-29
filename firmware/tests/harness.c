#include "harness.h"

#include <stdio.h>
#include <string.h>

int g_failed;
int g_passed;

void expect_eq(const char *file, int line, long got, long want, const char *expr) {
    if (got == want) {
        g_passed++;
        return;
    }
    fprintf(stderr, "FAIL %s:%d %s got %ld want %ld\n", file, line, expr, got, want);
    g_failed++;
}

void expect_true(const char *file, int line, int cond, const char *expr) {
    if (cond) {
        g_passed++;
        return;
    }
    fprintf(stderr, "FAIL %s:%d %s\n", file, line, expr);
    g_failed++;
}

void expect_str(const char *file, int line, const char *got, const char *prefix) {
    if (got != 0 && strncmp(got, prefix, strlen(prefix)) == 0) {
        g_passed++;
        return;
    }
    fprintf(stderr, "FAIL %s:%d prefix \"%s\" not in \"%s\"\n", file, line, prefix,
            got != 0 ? got : "(null)");
    g_failed++;
}
