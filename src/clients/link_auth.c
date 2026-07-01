#include "hybbx/link.h"
#include "hybbx/util.h"

#include <stdio.h>
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

    return HYBBX_OK;
}
