#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_failures;

static void check_int(const char *label, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", label, got, want);
        g_failures++;
    }
}

static void check_str(const char *label, const char *got, const char *want)
{
    if (got == NULL || want == NULL || strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s: got '%s' want '%s'\n",
                label, got != NULL ? got : "(null)", want);
        g_failures++;
    }
}

int main(void)
{
    check_int("true yes", hybbx_bool_is_true("yes"), 1);
    check_int("true enable", hybbx_bool_is_true("enable"), 1);
    check_int("false no", hybbx_bool_is_false("no"), 1);
    check_int("false disable", hybbx_bool_is_false("disable"), 1);

    check_int("parse yes", hybbx_parse_bool("yes", 0), 1);
    check_int("parse false", hybbx_parse_bool("false", 1), 0);
    check_int("parse empty", hybbx_parse_bool("", 1), 1);
    check_int("parse garbage", hybbx_parse_bool("maybe", 0), 0);

    check_str("bool yes", hybbx_bool_to_string(1), HYBBX_BOOL_YES);
    check_str("bool no", hybbx_bool_to_string(0), HYBBX_BOOL_NO);

    check_str("result ok", hybbx_result_name(HYBBX_OK), "ok");
    check_str("result invalid", hybbx_result_name(HYBBX_ERR_INVALID), "invalid");
    check_str("result unknown", hybbx_result_name((hybbx_result_t)-99), "unknown");

    if (g_failures != 0) {
        fprintf(stderr, "%u test(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }

    puts("test_util: ok");
    return EXIT_SUCCESS;
}
