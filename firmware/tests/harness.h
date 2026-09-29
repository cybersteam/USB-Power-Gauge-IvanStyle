#ifndef USB_POWER_GAUGE_HARNESS_H
#define USB_POWER_GAUGE_HARNESS_H

extern int g_failed;
extern int g_passed;

void expect_eq(const char *file, int line, long got, long want, const char *expr);
void expect_true(const char *file, int line, int cond, const char *expr);
void expect_str(const char *file, int line, const char *got, const char *prefix);

#define EXPECT_EQ(got, want) expect_eq(__FILE__, __LINE__, (long)(got), (long)(want), #got)
#define EXPECT_TRUE(cond) expect_true(__FILE__, __LINE__, (int)(cond), #cond)
#define EXPECT_PREFIX(got, prefix) expect_str(__FILE__, __LINE__, (got), (prefix))

#endif
