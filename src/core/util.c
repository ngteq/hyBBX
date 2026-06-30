#include "hybbx/util.h"
#include "hybbx/limits.h"

#include <stdio.h>
#include <string.h>

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
