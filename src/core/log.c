#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "hybbx/log.h"
#if !defined(HYBBX_CLIENT_BUILD)
#include "hybbx/config.h"
#endif
#include "hybbx/limits.h"
#include "hybbx/util.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#define HYBBX_LOG_LINE_MAX 1024u

static hybbx_log_config_t g_log_config;
static int g_log_ready;
static FILE *g_log_file;
static int g_log_open_yyyymm;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static int mkdir_p(const char *path)
{
    char buf[HYBBX_PATH_MAX];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    hybbx_strlcpy(buf, path, sizeof(buf));
    len = strlen(buf);
    while (len > 0 && buf[len - 1] == '/') {
        buf[--len] = '\0';
    }

    for (i = 1; i < len; i++) {
        if (buf[i] != '/') {
            continue;
        }
        buf[i] = '\0';
        if (buf[0] != '\0' && mkdir(buf, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
        buf[i] = '/';
    }

    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

void hybbx_log_config_defaults(hybbx_log_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->enabled = 1;
    cfg->dir[0] = '\0';
    cfg->level = HYBBX_LOG_WARN;
}

static hybbx_log_level_t log_parse_level(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_LOG_WARN;
    }

    if (strcasecmp(value, "debug") == 0) {
        return HYBBX_LOG_DEBUG;
    }
    if (strcasecmp(value, "info") == 0) {
        return HYBBX_LOG_INFO;
    }
    if (strcasecmp(value, "stats") == 0) {
        return HYBBX_LOG_STATS;
    }
    if (strcasecmp(value, "warn") == 0) {
        return HYBBX_LOG_WARN;
    }

    return HYBBX_LOG_WARN;
}

const char *hybbx_log_level_name(hybbx_log_level_t level)
{
    switch (level) {
    case HYBBX_LOG_DEBUG:
        return "debug";
    case HYBBX_LOG_INFO:
        return "info";
    case HYBBX_LOG_STATS:
        return "stats";
    case HYBBX_LOG_WARN:
        return "warn";
    default:
        return "?";
    }
}

const hybbx_log_config_t *hybbx_log_config_get(void)
{
    return g_log_ready ? &g_log_config : NULL;
}

int hybbx_log_enabled(void)
{
    return g_log_ready && g_log_config.enabled && g_log_file != NULL;
}

static void log_close_file(void)
{
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    g_log_open_yyyymm = 0;
}

static int log_build_path(char *out, size_t out_len, const struct tm *tm)
{
    char name[48];

    snprintf(name, sizeof(name), "%04d%02d%02d-hybbx.log",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return hybbx_path_join(out, out_len, g_log_config.dir, name) == HYBBX_OK ? 0 : -1;
}

static int log_open_for_time(const struct tm *tm)
{
    char path[HYBBX_PATH_MAX];
    FILE *fp;
    int yyyymm;

    if (tm == NULL) {
        return -1;
    }

    yyyymm = (tm->tm_year + 1900) * 100 + (tm->tm_mon + 1);
    if (g_log_file != NULL && g_log_open_yyyymm == yyyymm) {
        return 0;
    }

    log_close_file();

    if (mkdir_p(g_log_config.dir) != 0) {
        fprintf(stderr, "[log] cannot create directory %s\n", g_log_config.dir);
        return -1;
    }

    if (log_build_path(path, sizeof(path), tm) != 0) {
        fprintf(stderr, "[log] path too long for log file\n");
        return -1;
    }

    fp = fopen(path, "a");
    if (fp == NULL) {
        fprintf(stderr, "[log] cannot open %s\n", path);
        return -1;
    }

    g_log_file = fp;
    g_log_open_yyyymm = yyyymm;
    return 0;
}

static int log_ensure_open(void)
{
    time_t now;
    struct tm tm_buf;
    struct tm *tm;

    if (!g_log_config.enabled || g_log_config.dir[0] == '\0') {
        return -1;
    }

    now = time(NULL);
    tm = localtime_r(&now, &tm_buf);
    if (tm == NULL) {
        return -1;
    }

    return log_open_for_time(tm);
}

#if !defined(HYBBX_CLIENT_BUILD)
void hybbx_log_config_apply(const struct hybbx_config *config)
{
    const char *dir_raw;
    const char *level_raw;

    hybbx_log_shutdown();
    hybbx_log_config_defaults(&g_log_config);

    if (config != NULL) {
        g_log_config.enabled =
            hybbx_config_get_bool(config, "log", "enabled", 1);
        dir_raw = hybbx_config_get(config, "log", "dir", NULL);
        level_raw = hybbx_config_get(config, "log", "level", "warn");

        g_log_config.level = log_parse_level(level_raw);

        if (dir_raw != NULL && dir_raw[0] != '\0') {
            if (hybbx_path_resolve(g_log_config.dir, sizeof(g_log_config.dir),
                                   dir_raw) != HYBBX_OK) {
                fprintf(stderr, "[log] invalid dir path\n");
                g_log_config.enabled = 0;
            }
        } else if (g_log_config.enabled) {
            if (hybbx_path_resolve(g_log_config.dir, sizeof(g_log_config.dir),
                                   HYBBX_DIR_LOGS) != HYBBX_OK) {
                fprintf(stderr, "[log] cannot resolve default log directory\n");
                g_log_config.enabled = 0;
            }
        }
    } else {
        if (hybbx_path_resolve(g_log_config.dir, sizeof(g_log_config.dir),
                               HYBBX_DIR_LOGS) != HYBBX_OK) {
            g_log_config.enabled = 0;
        }
    }

    g_log_ready = 1;

    if (g_log_config.enabled) {
        if (log_ensure_open() != 0) {
            g_log_config.enabled = 0;
        }
    }

    printf("[log] enabled=%s dir=%s level=%s\n",
           hybbx_bool_to_string(g_log_config.enabled),
           g_log_config.dir[0] != '\0' ? g_log_config.dir : "-",
           hybbx_log_level_name(g_log_config.level));
}
#else
void hybbx_log_config_apply(const struct hybbx_config *config)
{
    (void)config;
    hybbx_log_config_defaults(&g_log_config);
    g_log_ready = 1;
}
#endif

void hybbx_log_write(hybbx_log_level_t level, const char *fmt, ...)
{
    char message[HYBBX_LOG_LINE_MAX];
    char line[HYBBX_LOG_LINE_MAX + 64];
    va_list ap;
    time_t now;
    struct tm tm_buf;
    struct tm *tm;

    if (!g_log_ready || !g_log_config.enabled || fmt == NULL) {
        return;
    }

    if (level < g_log_config.level) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&g_log_lock);

    if (log_ensure_open() != 0) {
        pthread_mutex_unlock(&g_log_lock);
        return;
    }

    now = time(NULL);
    tm = localtime_r(&now, &tm_buf);
    if (tm == NULL) {
        pthread_mutex_unlock(&g_log_lock);
        return;
    }

    if (hybbx_time_format_stamp(line, sizeof(line), tm, NULL) != HYBBX_OK) {
        pthread_mutex_unlock(&g_log_lock);
        return;
    }

    {
        size_t stamp_len = strlen(line);
        snprintf(line + stamp_len, sizeof(line) - stamp_len,
                 " [%s] %s\n", hybbx_log_level_name(level), message);
    }

    fputs(line, g_log_file);
    fflush(g_log_file);

    pthread_mutex_unlock(&g_log_lock);
}

void hybbx_log_shutdown(void)
{
    pthread_mutex_lock(&g_log_lock);
    log_close_file();
    g_log_ready = 0;
    hybbx_log_config_defaults(&g_log_config);
    pthread_mutex_unlock(&g_log_lock);
}
