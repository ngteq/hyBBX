#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "hybbx/security.h"
#if !defined(HYBBX_CLIENT_BUILD)
#include "hybbx/config.h"
#endif
#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define HYBBX_SECURITY_LINE_MAX 512u

static char g_security_dir[HYBBX_PATH_MAX];
static int g_security_ready;
static FILE *g_security_file;
static pthread_mutex_t g_security_lock = PTHREAD_MUTEX_INITIALIZER;

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

static int security_open_file(void)
{
    char path[HYBBX_PATH_MAX];

    if (g_security_dir[0] == '\0') {
        return -1;
    }

    if (g_security_file != NULL) {
        return 0;
    }

    if (mkdir_p(g_security_dir) != 0) {
        hybbx_log_warn("[security] cannot create directory %s",
                g_security_dir);
        return -1;
    }

    if (hybbx_path_join(path, sizeof(path), g_security_dir,
                        HYBBX_SECURITY_LOG_FILE) != HYBBX_OK) {
        return -1;
    }

    g_security_file = fopen(path, "a");
    if (g_security_file == NULL) {
        hybbx_log_warn("[security] cannot open %s", path);
        return -1;
    }

    return 0;
}

#if !defined(HYBBX_CLIENT_BUILD)
void hybbx_security_log_config_apply(const struct hybbx_config *config)
{
    const char *dir_raw;

    hybbx_security_log_shutdown();
    g_security_dir[0] = '\0';
    g_security_ready = 0;

    if (config != NULL) {
        dir_raw = hybbx_config_get(config, "log", "dir", NULL);
        if (dir_raw != NULL && dir_raw[0] != '\0') {
            if (hybbx_path_resolve(g_security_dir, sizeof(g_security_dir),
                                   dir_raw) != HYBBX_OK) {
                hybbx_log_warn("[security] invalid log dir path");
                return;
            }
        } else {
            if (hybbx_path_resolve(g_security_dir, sizeof(g_security_dir),
                                   HYBBX_DIR_LOGS) != HYBBX_OK) {
                hybbx_log_warn("[security] cannot resolve default log dir");
                return;
            }
        }
    } else if (hybbx_path_resolve(g_security_dir, sizeof(g_security_dir),
                                    HYBBX_DIR_LOGS) != HYBBX_OK) {
        return;
    }

    g_security_ready = 1;

    if (security_open_file() != 0) {
        g_security_ready = 0;
        return;
    }

    hybbx_log_info("[security] log=%s/%s", g_security_dir, HYBBX_SECURITY_LOG_FILE);
    hybbx_security_log_write("startup");
}
#else
void hybbx_security_log_config_apply(const struct hybbx_config *config)
{
    (void)config;
}
#endif

void hybbx_security_log_write(const char *fmt, ...)
{
    char message[HYBBX_SECURITY_LINE_MAX];
    char line[HYBBX_SECURITY_LINE_MAX + 64];
    char stamp[32];
    va_list ap;
    time_t now;
    struct tm tm_buf;
    struct tm *tm;

    if (!g_security_ready || fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    now = time(NULL);
    tm = localtime_r(&now, &tm_buf);
    if (tm == NULL) {
        return;
    }

    if (strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm) == 0) {
        return;
    }

    snprintf(line, sizeof(line), "%s %s\n", stamp, message);

    pthread_mutex_lock(&g_security_lock);

    if (security_open_file() == 0) {
        fputs(line, g_security_file);
        fflush(g_security_file);
    }

    pthread_mutex_unlock(&g_security_lock);
}

void hybbx_security_log_shutdown(void)
{
    pthread_mutex_lock(&g_security_lock);

    if (g_security_file != NULL) {
        fclose(g_security_file);
        g_security_file = NULL;
    }

    pthread_mutex_unlock(&g_security_lock);
    g_security_ready = 0;
}
