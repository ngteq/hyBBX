#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "hybbx/util.h"
#include "hybbx/limits.h"
#include "hybbx/socket.h"
#if !defined(HYBBX_CLIENT_BUILD)
#include "hybbx/log.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined(_WIN32) && !defined(__AMIGA__)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#if !defined(_WIN32) && !defined(__AMIGA__)
#include <sys/utsname.h>
#endif

#if !defined(HYBBX_CLIENT_BUILD)
#include "hybbx/config.h"
#endif

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
    if (out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_strlcpy(out, HYBBX_DIR_DATA, out_len) >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

static char g_install_root[HYBBX_PATH_MAX];

void hybbx_install_root_set(const char *root)
{
    if (root == NULL || root[0] == '\0') {
        g_install_root[0] = '\0';
        return;
    }

    hybbx_strlcpy(g_install_root, root, sizeof(g_install_root));
}

const char *hybbx_install_root_get(void)
{
    return g_install_root[0] != '\0' ? g_install_root : NULL;
}

static int path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/') {
        return 1;
    }

#if defined(_WIN32) || defined(__CYGWIN__)
    if (((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return 1;
    }
#endif

    return 0;
}

hybbx_result_t hybbx_path_resolve(char *out, size_t out_len, const char *path)
{
    char expanded[HYBBX_PATH_MAX];
    const char *root;
    hybbx_result_t rc;

    if (out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_path_expand(expanded, sizeof(expanded), path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (path_is_absolute(expanded)) {
        if (hybbx_strlcpy(out, expanded, out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    root = hybbx_install_root_get();
    if (root != NULL && root[0] != '\0') {
        return hybbx_path_join(out, out_len, root, expanded);
    }

    if (hybbx_strlcpy(out, expanded, out_len) >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_path_dirname(const char *path, char *out, size_t out_len)
{
    char copy[HYBBX_PATH_MAX];
    char *slash;

    if (path == NULL || out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_strlcpy(copy, path, sizeof(copy)) >= sizeof(copy)) {
        return HYBBX_ERR_INVALID;
    }

    slash = strrchr(copy, '/');
    if (slash == NULL) {
        if (hybbx_strlcpy(out, ".", out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    if (slash == copy) {
        if (hybbx_strlcpy(out, "/", out_len) >= out_len) {
            return HYBBX_ERR_INVALID;
        }
        return HYBBX_OK;
    }

    *slash = '\0';
    if (hybbx_strlcpy(out, copy, out_len) >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_platform_os_name(char *out, size_t out_len)
{
    const char *name;

    if (out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

#if defined(_WIN32)
    name = "Windows";
#elif defined(__AMIGA__)
    name = "AmigaOS";
#else
    {
        struct utsname u;

        if (uname(&u) != 0) {
            return HYBBX_ERR_IO;
        }

        name = u.sysname;
        if (name == NULL || name[0] == '\0') {
            return HYBBX_ERR_IO;
        }

        if (strcmp(name, "Darwin") == 0) {
            name = "MacOS";
        }
    }
#endif

    if (hybbx_strlcpy(out, name, out_len) >= out_len) {
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

static hybbx_time_format_t g_time_format;
static int g_time_format_ready;

void hybbx_time_format_defaults(hybbx_time_format_t *fmt)
{
    if (fmt == NULL) {
        return;
    }

    fmt->clock_12h = 0;
    fmt->seconds = 1;
    fmt->date_format = HYBBX_DATE_ISO;
}

const char *hybbx_date_format_name(hybbx_date_format_t fmt)
{
    switch (fmt) {
    case HYBBX_DATE_ISO_SHORT:
        return "iso_short";
    case HYBBX_DATE_US:
        return "us";
    case HYBBX_DATE_EU:
        return "eu";
    case HYBBX_DATE_ISO:
    default:
        return "iso";
    }
}

#if !defined(HYBBX_CLIENT_BUILD)
static hybbx_date_format_t parse_date_format(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_DATE_ISO;
    }

    if (bool_token_ieq(value, "iso") || bool_token_ieq(value, "yyyy_mm_dd") ||
        bool_token_ieq(value, "yyyy/mm/dd")) {
        return HYBBX_DATE_ISO;
    }

    if (bool_token_ieq(value, "iso_short") || bool_token_ieq(value, "yy_mm_dd") ||
        bool_token_ieq(value, "yy/mm/dd")) {
        return HYBBX_DATE_ISO_SHORT;
    }

    if (bool_token_ieq(value, "us") || bool_token_ieq(value, "mm_dd_yyyy") ||
        bool_token_ieq(value, "mm/dd/yyyy")) {
        return HYBBX_DATE_US;
    }

    if (bool_token_ieq(value, "eu") || bool_token_ieq(value, "dd_mm_yyyy") ||
        bool_token_ieq(value, "dd/mm/yyyy")) {
        return HYBBX_DATE_EU;
    }

    return HYBBX_DATE_ISO;
}
#endif

hybbx_result_t hybbx_time_local_now(struct tm *out)
{
    time_t now;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return HYBBX_ERR_IO;
    }

#if defined(_WIN32)
    if (localtime_s(out, &now) != 0) {
        return HYBBX_ERR_IO;
    }
#else
    if (localtime_r(&now, out) == NULL) {
        return HYBBX_ERR_IO;
    }
#endif

    return HYBBX_OK;
}

const hybbx_time_format_t *hybbx_time_format_get(void)
{
    if (!g_time_format_ready) {
        hybbx_time_format_defaults(&g_time_format);
        g_time_format_ready = 1;
    }

    return &g_time_format;
}

#if !defined(HYBBX_CLIENT_BUILD)
void hybbx_time_config_apply(const struct hybbx_config *config)
{
    const char *clock;
    int clock_12h;

    hybbx_time_format_defaults(&g_time_format);

    if (config != NULL) {
        clock_12h = hybbx_config_get_bool(config, "time", "clock_12h", 0);
        if (!clock_12h) {
            clock_12h = hybbx_config_get_bool(config, "time", "am_pm", 0);
        }

        clock = hybbx_config_get(config, "time", "clock", NULL);
        if (clock != NULL && clock[0] != '\0') {
            if (bool_token_ieq(clock, "12h") || bool_token_ieq(clock, "12") ||
                bool_token_ieq(clock, "am_pm") || bool_token_ieq(clock, "am/pm")) {
                clock_12h = 1;
            } else if (bool_token_ieq(clock, "24h") || bool_token_ieq(clock, "24")) {
                clock_12h = 0;
            }
        }

        g_time_format.clock_12h = clock_12h;
        g_time_format.seconds =
            hybbx_config_get_bool(config, "time", "seconds", 1);
        g_time_format.date_format = parse_date_format(
            hybbx_config_get(config, "time", "date", NULL));
    }

    g_time_format_ready = 1;

    hybbx_log_info("[time] clock=%s seconds=%s date=%s",
           g_time_format.clock_12h ? "12h" : "24h",
           hybbx_bool_to_string(g_time_format.seconds),
           hybbx_date_format_name(g_time_format.date_format));
}
#else
void hybbx_time_config_apply(const struct hybbx_config *config)
{
    (void)config;
    hybbx_time_format_defaults(&g_time_format);
    g_time_format_ready = 1;
}
#endif

hybbx_result_t hybbx_time_format_time(char *out, size_t out_len,
                                      const struct tm *tm,
                                      const hybbx_time_format_t *fmt)
{
    int hour;
    int minute;
    int second;
    const char *suffix;
    int n;

    if (out == NULL || out_len == 0 || tm == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (fmt == NULL) {
        fmt = hybbx_time_format_get();
    }

    hour = tm->tm_hour;
    minute = tm->tm_min;
    second = tm->tm_sec;
    suffix = "";

    if (fmt->clock_12h) {
        if (hour >= 12) {
            suffix = "pm";
            if (hour > 12) {
                hour -= 12;
            }
        } else {
            suffix = "am";
            if (hour == 0) {
                hour = 12;
            }
        }
    }

    if (fmt->clock_12h) {
        if (fmt->seconds) {
            n = snprintf(out, out_len, "%02d:%02d:%02d %s",
                         hour, minute, second, suffix);
        } else {
            n = snprintf(out, out_len, "%02d:%02d %s", hour, minute, suffix);
        }
    } else if (fmt->seconds) {
        n = snprintf(out, out_len, "%02d:%02d:%02d", hour, minute, second);
    } else {
        n = snprintf(out, out_len, "%02d:%02d", hour, minute);
    }

    if (n < 0 || (size_t)n >= out_len) {
        out[0] = '\0';
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_time_format_date(char *out, size_t out_len,
                                      const struct tm *tm,
                                      const hybbx_time_format_t *fmt)
{
    int year;
    int month;
    int day;
    int n;

    if (out == NULL || out_len == 0 || tm == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (fmt == NULL) {
        fmt = hybbx_time_format_get();
    }

    year = tm->tm_year + 1900;
    month = tm->tm_mon + 1;
    day = tm->tm_mday;

    switch (fmt->date_format) {
    case HYBBX_DATE_ISO_SHORT:
        n = snprintf(out, out_len, "%02d/%02d/%02d",
                     year % 100, month, day);
        break;
    case HYBBX_DATE_US:
        n = snprintf(out, out_len, "%02d/%02d/%04d", month, day, year);
        break;
    case HYBBX_DATE_EU:
        n = snprintf(out, out_len, "%02d/%02d/%04d", day, month, year);
        break;
    case HYBBX_DATE_ISO:
    default:
        n = snprintf(out, out_len, "%04d/%02d/%02d", year, month, day);
        break;
    }

    if (n < 0 || (size_t)n >= out_len) {
        out[0] = '\0';
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_time_format_stamp(char *out, size_t out_len,
                                       const struct tm *tm,
                                       const hybbx_time_format_t *fmt)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    const char *suffix;
    int n;

    if (out == NULL || out_len == 0 || tm == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (fmt == NULL) {
        fmt = hybbx_time_format_get();
    }

    year = tm->tm_year + 1900;
    month = tm->tm_mon + 1;
    day = tm->tm_mday;
    hour = tm->tm_hour;
    minute = tm->tm_min;
    second = tm->tm_sec;
    suffix = "";

    if (fmt->clock_12h) {
        if (hour >= 12) {
            suffix = "pm";
            if (hour > 12) {
                hour -= 12;
            }
        } else {
            suffix = "am";
            if (hour == 0) {
                hour = 12;
            }
        }
    }

    if (fmt->clock_12h) {
        if (fmt->seconds) {
            n = snprintf(out, out_len, "%04d%02d%02d %02d:%02d:%02d %s",
                         year, month, day, hour, minute, second, suffix);
        } else {
            n = snprintf(out, out_len, "%04d%02d%02d %02d:%02d %s",
                         year, month, day, hour, minute, suffix);
        }
    } else if (fmt->seconds) {
        n = snprintf(out, out_len, "%04d%02d%02d %02d:%02d:%02d",
                     year, month, day, hour, minute, second);
    } else {
        n = snprintf(out, out_len, "%04d%02d%02d %02d:%02d",
                     year, month, day, hour, minute);
    }

    if (n < 0 || (size_t)n >= out_len) {
        out[0] = '\0';
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

void hybbx_socket_log_bind_failure(const char *component, const char *addr,
                                   unsigned port)
{
    int err = errno;

    if (component == NULL || addr == NULL) {
        return;
    }

#if !defined(HYBBX_CLIENT_BUILD)
    hybbx_log_warn("[%s] failed to bind %s:%u (%s)", component, addr, port,
                   strerror(err));
    if (err == EADDRINUSE) {
        hybbx_log_warn("[%s] port %u already in use — another HyBBX instance may "
                       "already be running (ss -tlnp | grep %u)",
                       component, port, port);
    }
#else
    fprintf(stderr, "[%s] failed to bind %s:%u (%s)\n", component, addr, port,
            strerror(err));
    if (err == EADDRINUSE) {
        fprintf(stderr,
                "[%s] port %u already in use — another HyBBX instance may "
                "already be running (ss -tlnp | grep %u)\n",
                component, port, port);
    }
#endif
}

hybbx_result_t hybbx_socket_peer_name(int fd, char *buf, size_t buf_len)
{
#if defined(_WIN32) || defined(__AMIGA__)
    (void)fd;
    (void)buf;
    (void)buf_len;
    return HYBBX_ERR_UNSUPPORTED;
#else
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    const void *ip;
    char ip_buf[INET6_ADDRSTRLEN];

    if (buf == NULL || buf_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    buf[0] = '\0';

    if (fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) != 0) {
        return HYBBX_ERR_IO;
    }

    if (addr.ss_family == AF_INET) {
        const struct sockaddr_in *in4 = (const struct sockaddr_in *)&addr;

        ip = &in4->sin_addr;
    } else if (addr.ss_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)&addr;

        ip = &in6->sin6_addr;
    } else {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (inet_ntop(addr.ss_family, ip, ip_buf, sizeof(ip_buf)) == NULL) {
        return HYBBX_ERR_IO;
    }

    hybbx_strlcpy(buf, ip_buf, buf_len);
    return HYBBX_OK;
#endif
}
