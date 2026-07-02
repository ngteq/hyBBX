#include "hybbx/limits.h"
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

    {
        char path[HYBBX_PATH_MAX];
        const char *home = getenv("HOME");

        if (hybbx_path_expand(path, sizeof(path), NULL) == HYBBX_OK) {
            check_str("path expand null", path, HYBBX_DIR_DATA);
        }
        if (home != NULL && home[0] != '\0') {
            char want[HYBBX_PATH_MAX];

            snprintf(want, sizeof(want), "%s/custom", home);
            if (hybbx_path_expand(path, sizeof(path), "~/custom") == HYBBX_OK) {
                check_str("path expand tilde", path, want);
            }
            if (hybbx_path_expand(path, sizeof(path), "~") == HYBBX_OK) {
                check_str("path expand home only", path, home);
            }
        }
    }

    {
        char path[HYBBX_PATH_MAX];
        char want[HYBBX_PATH_MAX];

        hybbx_install_root_set("/opt/hybbx");
        if (hybbx_path_resolve(path, sizeof(path), HYBBX_DIR_DATA) == HYBBX_OK) {
            snprintf(want, sizeof(want), "/opt/hybbx/%s", HYBBX_DIR_DATA);
            check_str("path resolve data", path, want);
        }
        if (hybbx_path_resolve(path, sizeof(path), HYBBX_DIR_LOGS) == HYBBX_OK) {
            snprintf(want, sizeof(want), "/opt/hybbx/%s", HYBBX_DIR_LOGS);
            check_str("path resolve logs", path, want);
        }
        hybbx_install_root_set(NULL);
    }

    {
        char os_name[HYBBX_PATH_MAX];

        if (hybbx_platform_os_name(os_name, sizeof(os_name)) != HYBBX_OK) {
            fprintf(stderr, "FAIL platform os name: lookup failed\n");
            g_failures++;
        } else if (os_name[0] == '\0') {
            fprintf(stderr, "FAIL platform os name: empty\n");
            g_failures++;
#if defined(__linux__)
        } else if (strcmp(os_name, "Linux") != 0) {
            fprintf(stderr, "FAIL platform os name: got '%s' want 'Linux'\n",
                    os_name);
            g_failures++;
#endif
        }
    }

    if (g_failures != 0) {
        fprintf(stderr, "%u test(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }

    puts("test_util: ok");
    return EXIT_SUCCESS;
}
