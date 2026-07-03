#include "hybbx/link.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_kv_line(const char *payload, size_t len,
                                const char *key,
                                char *scratch, size_t scratch_len)
{
    const char *cursor = payload;
    const char *end = payload + len;
    size_t key_len = strlen(key);

    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        const char *line_stop = line_end != NULL ? line_end : end;
        const char *eq = memchr(cursor, '=', (size_t)(line_stop - cursor));

        if (eq != NULL && (size_t)(eq - cursor) == key_len &&
            memcmp(cursor, key, key_len) == 0) {
            const char *value = eq + 1;
            size_t value_len = (size_t)(line_stop - value);

            while (value_len > 0 &&
                   (value[value_len - 1] == '\r' ||
                    value[value_len - 1] == '\n' ||
                    value[value_len - 1] == ' ')) {
                value_len--;
            }
            if (value_len >= scratch_len) {
                value_len = scratch_len - 1;
            }
            memcpy(scratch, value, value_len);
            scratch[value_len] = '\0';
            return scratch;
        }

        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    return NULL;
}

void hybbx_link_auth_clear(hybbx_link_auth_t *auth)
{
    if (auth == NULL) {
        return;
    }

    memset(auth, 0, sizeof(*auth));
}

size_t hybbx_link_auth_format(const hybbx_link_auth_t *auth,
                              char *out, size_t out_cap)
{
    int n;

    if (auth == NULL || out == NULL || out_cap == 0) {
        return 0;
    }

    if (auth->password[0] == '\0' || auth->id[0] == '\0') {
        return 0;
    }

    n = snprintf(out, out_cap,
                 "password=%s\nrole=%s\nid=%s\n",
                 auth->password,
                 auth->role[0] != '\0' ? auth->role : "link",
                 auth->id);
    if (n < 0 || (size_t)n >= out_cap) {
        return 0;
    }

    if (auth->baud > 0) {
        int extra = snprintf(out + (size_t)n, out_cap - (size_t)n,
                             "baud=%u\n", auth->baud);
        if (extra < 0 || (size_t)n + (size_t)extra >= out_cap) {
            return 0;
        }
        n += extra;
    }

    if (auth->duplex == 1) {
        int extra = snprintf(out + (size_t)n, out_cap - (size_t)n,
                             "duplex=half\n");
        if (extra < 0 || (size_t)n + (size_t)extra >= out_cap) {
            return 0;
        }
        n += extra;
    } else if (auth->duplex == 2) {
        int extra = snprintf(out + (size_t)n, out_cap - (size_t)n,
                             "duplex=full\n");
        if (extra < 0 || (size_t)n + (size_t)extra >= out_cap) {
            return 0;
        }
        n += extra;
    }

    if (auth->bandwidth[0] != '\0') {
        int extra = snprintf(out + (size_t)n, out_cap - (size_t)n,
                             "bandwidth=%s\n", auth->bandwidth);
        if (extra < 0 || (size_t)n + (size_t)extra >= out_cap) {
            return 0;
        }
        n += extra;
    }

    if (auth->frequency_mhz[0] != '\0') {
        int extra = snprintf(out + (size_t)n, out_cap - (size_t)n,
                             "frequency_mhz=%s\n", auth->frequency_mhz);
        if (extra < 0 || (size_t)n + (size_t)extra >= out_cap) {
            return 0;
        }
        n += extra;
    }

    return (size_t)n;
}

hybbx_result_t hybbx_link_auth_parse(const char *payload, size_t len,
                                     hybbx_link_auth_t *auth)
{
    char scratch[128];

    if (payload == NULL || auth == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_link_auth_clear(auth);

    if (find_kv_line(payload, len, "password", scratch, sizeof(scratch)) == NULL) {
        return HYBBX_ERR_INVALID;
    }
    hybbx_strlcpy(auth->password, scratch, sizeof(auth->password));

    if (find_kv_line(payload, len, "id", scratch, sizeof(scratch)) == NULL) {
        return HYBBX_ERR_INVALID;
    }
    hybbx_strlcpy(auth->id, scratch, sizeof(auth->id));

    if (find_kv_line(payload, len, "role", scratch, sizeof(scratch)) != NULL) {
        hybbx_strlcpy(auth->role, scratch, sizeof(auth->role));
    } else {
        hybbx_strlcpy(auth->role, "link", sizeof(auth->role));
    }

    if (find_kv_line(payload, len, "baud", scratch, sizeof(scratch)) != NULL) {
        unsigned long baud = strtoul(scratch, NULL, 10);
        if (baud > 0) {
            auth->baud = (unsigned)baud;
        }
    }

    if (find_kv_line(payload, len, "duplex", scratch, sizeof(scratch)) != NULL) {
        if (strcmp(scratch, "full") == 0 || strcmp(scratch, "full-duplex") == 0) {
            auth->duplex = 2;
        } else if (strcmp(scratch, "half") == 0 ||
                   strcmp(scratch, "half-duplex") == 0 ||
                   strcmp(scratch, "simplex") == 0) {
            auth->duplex = 1;
        }
    }

    if (find_kv_line(payload, len, "bandwidth", scratch, sizeof(scratch)) != NULL) {
        hybbx_strlcpy(auth->bandwidth, scratch, sizeof(auth->bandwidth));
    }

    if (find_kv_line(payload, len, "frequency_mhz", scratch, sizeof(scratch)) != NULL) {
        hybbx_strlcpy(auth->frequency_mhz, scratch, sizeof(auth->frequency_mhz));
    } else if (find_kv_line(payload, len, "frequency", scratch,
                            sizeof(scratch)) != NULL) {
        hybbx_strlcpy(auth->frequency_mhz, scratch, sizeof(auth->frequency_mhz));
    }

    return HYBBX_OK;
}
