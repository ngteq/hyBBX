#include "hybbx/crdop.h"

#include <ctype.h>
#include <string.h>

static int str_ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = (char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);
        char cb = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);

        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

hybbx_crdop_radio_profile_t hybbx_crdop_profile_parse(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_CRDOP_PROFILE_AMATEUR;
    }

    if (str_ieq(value, "cb") || str_ieq(value, "11m") ||
        str_ieq(value, "citizens_band")) {
        return HYBBX_CRDOP_PROFILE_CB;
    }

    return HYBBX_CRDOP_PROFILE_AMATEUR;
}

const char *hybbx_crdop_profile_name(hybbx_crdop_radio_profile_t profile)
{
    return profile == HYBBX_CRDOP_PROFILE_CB ? "cb" : "amateur";
}

const char *hybbx_crdop_default_arq_bandwidth(hybbx_crdop_radio_profile_t profile)
{
    return profile == HYBBX_CRDOP_PROFILE_CB ? "500MAX" : "500MAX";
}

int hybbx_crdop_bandwidth_exceeds_cb(const char *arq_bandwidth)
{
    const char *p;
    unsigned n = 0;

    if (arq_bandwidth == NULL || arq_bandwidth[0] == '\0') {
        return 0;
    }

    p = arq_bandwidth;
    while (*p >= '0' && *p <= '9') {
        n = n * 10u + (unsigned)(*p - '0');
        p++;
    }

    return n > 1000u;
}
