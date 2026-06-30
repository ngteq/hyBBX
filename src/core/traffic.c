#include "hybbx/traffic.h"
#include "hybbx/config.h"
#include "hybbx/session.h"
#include "hybbx/terminal.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/select.h>
#endif

static hybbx_traffic_config_t g_traffic_config;
static int g_traffic_config_ready;

void hybbx_traffic_config_defaults(hybbx_traffic_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->baud = HYBBX_BAUD2400;
    cfg->line_width = HYBBX_LINE_WIDTH;
    cfg->pace_output = 1;
    cfg->ansi = 0;
}

static unsigned parse_uint_default(const hybbx_config_t *config,
                                   const char *section,
                                   const char *key,
                                   unsigned default_value,
                                   unsigned min_value,
                                   unsigned max_value)
{
    unsigned value;

    value = hybbx_config_get_uint(config, section, key, default_value,
                                  min_value, max_value);
    return value;
}

void hybbx_traffic_config_apply(const hybbx_config_t *config)
{
    hybbx_traffic_config_defaults(&g_traffic_config);

    if (config != NULL) {
        g_traffic_config.baud = parse_uint_default(
            config, "traffic", "baud", HYBBX_BAUD2400, 300u, 38400u);
        g_traffic_config.line_width = parse_uint_default(
            config, "traffic", "line_width", HYBBX_LINE_WIDTH, 20u, 80u);
        g_traffic_config.pace_output =
            hybbx_config_get_bool(config, "traffic", "pace_output", 1);
        g_traffic_config.ansi =
            hybbx_config_get_bool(config, "traffic", "ansi", 0);
    }

    g_traffic_config_ready = 1;

    printf("[traffic] baud=%u line_width=%u pace=%s ansi=%s\n",
           g_traffic_config.baud, g_traffic_config.line_width,
           g_traffic_config.pace_output ? "yes" : "no",
           g_traffic_config.ansi ? "yes" : "no");
}

const hybbx_traffic_config_t *hybbx_traffic_config_get(void)
{
    if (!g_traffic_config_ready) {
        hybbx_traffic_config_defaults(&g_traffic_config);
        g_traffic_config_ready = 1;
    }

    return &g_traffic_config;
}

unsigned hybbx_traffic_byte_delay_us(unsigned baud)
{
    if (baud == 0) {
        return 0;
    }

    return 10000000u / baud;
}

static hybbx_result_t emit_raw(struct hybbx_session *session, char ch)
{
    const hybbx_traffic_config_t *cfg = hybbx_traffic_config_get();
    char byte = ch;
    hybbx_result_t rc;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (session->transport != NULL && session->transport->write != NULL) {
        rc = session->transport->write(session, &byte, 1);
    } else {
        if (fputc((unsigned char)byte, stdout) == EOF) {
            return HYBBX_ERR_IO;
        }
        fflush(stdout);
        rc = HYBBX_OK;
    }

    if (rc == HYBBX_OK && cfg->pace_output && cfg->baud > 0) {
        unsigned delay = hybbx_traffic_byte_delay_us(cfg->baud);

        if (delay > 0) {
#if defined(_WIN32)
            Sleep((DWORD)((delay + 999u) / 1000u));
#else
            struct timeval tv;

            tv.tv_sec = (time_t)(delay / 1000000u);
            tv.tv_usec = (suseconds_t)(delay % 1000000u);
            (void)select(0, NULL, NULL, NULL, &tv);
#endif
        }
    }

    return rc;
}

static hybbx_result_t emit_newline(struct hybbx_session *session,
                                   unsigned *out_col)
{
    hybbx_result_t rc;

    rc = emit_raw(session, '\r');
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = emit_raw(session, '\n');
    if (rc == HYBBX_OK && out_col != NULL) {
        *out_col = 0;
    }

    return rc;
}

static int traffic_plain_char(const char *src, size_t len, size_t *consumed,
                              char *out)
{
    unsigned char ch;

    if (src == NULL || len == 0 || consumed == NULL || out == NULL) {
        return 0;
    }

    ch = (unsigned char)src[0];

    if (ch == '\r') {
        *consumed = 1;
        *out = '\n';
        return 1;
    }

    if (ch == '\n') {
        *consumed = 1;
        *out = '\n';
        return 1;
    }

    if (ch == '\t') {
        *consumed = 1;
        *out = ' ';
        return 1;
    }

    if (ch < 0x20 || ch == 0x7f) {
        *consumed = 1;
        return 0;
    }

    if (ch == 0x1b && len >= 2 && src[1] == '[') {
        size_t i = 2;

        while (i < len && (src[i] < 0x40 || src[i] > 0x7e)) {
            i++;
        }
        if (i < len) {
            *consumed = i + 1;
        } else {
            *consumed = len;
        }
        return 0;
    }

    if (ch == 0x1b) {
        *consumed = 1;
        return 0;
    }

    *consumed = 1;
    *out = (char)ch;
    return 1;
}

hybbx_result_t hybbx_traffic_emit(struct hybbx_session *session,
                                  unsigned *out_col,
                                  const char *data, size_t len)
{
    const hybbx_traffic_config_t *cfg = hybbx_traffic_config_get();
    size_t pos = 0;
    unsigned col = out_col != NULL ? *out_col : 0;

    if (session == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (len == 0) {
        return HYBBX_OK;
    }

    if (cfg->ansi && strcmp(data, HYBBX_TERM_SGR_LIGHTGRAY_ON_BLACK) == 0 &&
        len == strlen(HYBBX_TERM_SGR_LIGHTGRAY_ON_BLACK)) {
        size_t i;

        for (i = 0; i < len; i++) {
            hybbx_result_t rc = emit_raw(session, data[i]);

            if (rc != HYBBX_OK) {
                return rc;
            }
        }

        return HYBBX_OK;
    }

    while (pos < len) {
        char ch;
        size_t step = 0;
        hybbx_result_t rc;

        if (!traffic_plain_char(data + pos, len - pos, &step, &ch)) {
            pos += step > 0 ? step : 1;
            continue;
        }

        pos += step;

        if (ch == '\n') {
            rc = emit_newline(session, out_col);
            if (rc != HYBBX_OK) {
                return rc;
            }
            col = 0;
            continue;
        }

        if (cfg->line_width > 0 && col >= cfg->line_width) {
            rc = emit_newline(session, out_col);
            if (rc != HYBBX_OK) {
                return rc;
            }
            col = 0;
        }

        rc = emit_raw(session, ch);
        if (rc != HYBBX_OK) {
            return rc;
        }

        col++;
    }

    if (out_col != NULL) {
        *out_col = col;
    }

    return HYBBX_OK;
}
