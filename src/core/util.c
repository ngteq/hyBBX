#include "hybbx/util.h"
#include "hybbx/limits.h"
#include "hybbx/socket.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int bool_token_ieq(const char *value, const char *token)
{
    if (value == NULL || token == NULL) {
        return 0;
    }

    while (*value != '\0' && *token != '\0') {
        unsigned char cv = (unsigned char)tolower((unsigned char)*value);
        unsigned char ct = (unsigned char)tolower((unsigned char)*token);

        if (cv != ct) {
            return 0;
        }
        value++;
        token++;
    }

    return *value == '\0' && *token == '\0';
}

static int bool_match_any(const char *value, const char *const *tokens, size_t count)
{
    size_t i;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (bool_token_ieq(value, tokens[i])) {
            return 1;
        }
    }

    return 0;
}

static const char *const hybbx_bool_true_tokens[] = {
    HYBBX_BOOL_YES, "true", "enable", "enabled", "on", "1"
};

static const char *const hybbx_bool_false_tokens[] = {
    HYBBX_BOOL_NO, "false", "disable", "disabled", "off", "0"
};

int hybbx_bool_is_true(const char *value)
{
    return bool_match_any(value, hybbx_bool_true_tokens,
                          sizeof(hybbx_bool_true_tokens) /
                              sizeof(hybbx_bool_true_tokens[0]));
}

int hybbx_bool_is_false(const char *value)
{
    return bool_match_any(value, hybbx_bool_false_tokens,
                          sizeof(hybbx_bool_false_tokens) /
                              sizeof(hybbx_bool_false_tokens[0]));
}

int hybbx_parse_bool(const char *value, int default_value)
{
    if (value == NULL || value[0] == '\0') {
        return default_value;
    }

    if (hybbx_bool_is_true(value)) {
        return 1;
    }

    if (hybbx_bool_is_false(value)) {
        return 0;
    }

    return default_value;
}

const char *hybbx_bool_to_string(int value)
{
    return value ? HYBBX_BOOL_YES : HYBBX_BOOL_NO;
}

const char *hybbx_result_name(hybbx_result_t rc)
{
    switch (rc) {
    case HYBBX_OK:
        return "ok";
    case HYBBX_LOCAL_CMD:
        return "local_cmd";
    case HYBBX_SESSION_END:
        return "session_end";
    case HYBBX_ERR_INVALID:
        return "invalid";
    case HYBBX_ERR_NOMEM:
        return "nomem";
    case HYBBX_ERR_NOT_FOUND:
        return "not_found";
    case HYBBX_ERR_IO:
        return "io";
    case HYBBX_ERR_UNSUPPORTED:
        return "unsupported";
    case HYBBX_ERR_BUSY:
        return "busy";
    case HYBBX_ERR_DENIED:
        return "denied";
    default:
        return "unknown";
    }
}

size_t hybbx_strlcpy(char *dst, const char *src, size_t dst_size)
{
    size_t src_len;

    if (dst_size == 0) {
        return src != NULL ? strlen(src) : 0;
    }

    if (dst == NULL) {
        return 0;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }

    src_len = strlen(src);
    if (src_len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return dst_size - 1;
    }

    memcpy(dst, src, src_len + 1);
    return src_len;
}

hybbx_result_t hybbx_default_user_data_path(char *out, size_t out_len)
{
    const char *home;

    if (out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        if (snprintf(out, out_len, "%s/.hybbx", home) >= (int)out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    if (hybbx_strlcpy(out, ".hybbx", out_len) >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_path_expand(char *out, size_t out_len, const char *path)
{
    const char *home;

    if (out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (path == NULL || path[0] == '\0') {
        return hybbx_default_user_data_path(out, out_len);
    }

    if (path[0] != '~') {
        if (hybbx_strlcpy(out, path, out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    if (path[1] != '\0' && path[1] != '/') {
        return HYBBX_ERR_INVALID;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        if (path[1] == '\0') {
            return hybbx_default_user_data_path(out, out_len);
        }
        if (hybbx_strlcpy(out, path + 2, out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    if (path[1] == '\0') {
        if (hybbx_strlcpy(out, home, out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    if (snprintf(out, out_len, "%s%s", home, path + 1) >= (int)out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

int hybbx_size_ok(size_t len)
{
    return len > 0 && len <= HYBBX_ALLOC_MAX;
}

static int path_component_valid(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }

    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        return 0;
    }

    return 1;
}

hybbx_result_t hybbx_path_join(char *out, size_t out_len,
                               const char *base, const char *name)
{
    size_t base_len;
    int need_slash;
    int written;

    if (out == NULL || out_len == 0 || base == NULL || name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!path_component_valid(name)) {
        return HYBBX_ERR_INVALID;
    }

    base_len = strlen(base);
    if (!hybbx_size_ok(base_len + 1) || !hybbx_size_ok(strlen(name) + 1)) {
        return HYBBX_ERR_INVALID;
    }

    need_slash = base_len > 0 && base[base_len - 1] != '/';
    written = snprintf(out, out_len, "%s%s%s", base, need_slash ? "/" : "", name);
    if (written < 0 || (size_t)written >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

void hybbx_socket_nosigpipe(int fd)
{
#ifdef SO_NOSIGPIPE
    int on = 1;

    if (fd >= 0) {
        (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    }
#else
    (void)fd;
#endif
}
