#include "hybbx/texts.h"
#include "hybbx/session.h"
#include "hybbx/service.h"
#include "hybbx/auth.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"
#include "hybbx/hybbx.h"

#include <stdio.h>
#include <string.h>

void hybbx_texts_config_defaults(hybbx_texts_config_t *texts)
{
    if (texts == NULL) {
        return;
    }

    hybbx_strlcpy(texts->path, HYBBX_DEFAULT_TEXTS_PATH, sizeof(texts->path));
}

hybbx_result_t hybbx_texts_resolve(const hybbx_texts_config_t *texts,
                                   const char *filename,
                                   char *out, size_t out_len)
{
    if (texts == NULL || filename == NULL || out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, texts->path, filename);
}

static size_t append_token_expanded(char *out, size_t out_len, size_t pos,
                                    const char *value)
{
    size_t value_len;

    if (out == NULL || out_len == 0 || value == NULL) {
        return pos;
    }

    value_len = strlen(value);

    if (pos + value_len >= out_len) {
        return out_len - 1;
    }

    memcpy(out + pos, value, value_len);
    return pos + value_len;
}

static void expand_text_line(char *out, size_t out_len,
                             const char *line,
                             const char *version,
                             const char *service_name,
                             const char *username)
{
    size_t pos = 0;
    size_t i = 0;

    if (out == NULL || out_len == 0 || line == NULL) {
        return;
    }

    out[0] = '\0';

    if (version == NULL) {
        version = HYBBX_VERSION_STRING;
    }
    if (service_name == NULL || service_name[0] == '\0') {
        service_name = HYBBX_DEFAULT_SERVICE_NAME;
    }
    if (username == NULL) {
        username = "";
    }

    while (line[i] != '\0' && pos + 1 < out_len) {
        if (strncmp(line + i, HYBBX_BANNER_TOKEN_VERSION,
                    strlen(HYBBX_BANNER_TOKEN_VERSION)) == 0) {
            pos = append_token_expanded(out, out_len, pos, version);
            i += strlen(HYBBX_BANNER_TOKEN_VERSION);
            continue;
        }

        if (strncmp(line + i, HYBBX_BANNER_TOKEN_SERVICE,
                    strlen(HYBBX_BANNER_TOKEN_SERVICE)) == 0) {
            pos = append_token_expanded(out, out_len, pos, service_name);
            i += strlen(HYBBX_BANNER_TOKEN_SERVICE);
            continue;
        }

        if (strncmp(line + i, HYBBX_TEXT_TOKEN_USERNAME,
                    strlen(HYBBX_TEXT_TOKEN_USERNAME)) == 0) {
            pos = append_token_expanded(out, out_len, pos, username);
            i += strlen(HYBBX_TEXT_TOKEN_USERNAME);
            continue;
        }

        out[pos++] = line[i++];
    }

    out[pos] = '\0';
}

static void expand_banner_line(char *out, size_t out_len,
                               const char *line,
                               const char *version,
                               const char *service_name)
{
    expand_text_line(out, out_len, line, version, service_name, NULL);
}

static hybbx_result_t send_banner_fallback(hybbx_session_t *session,
                                           const char *version,
                                           const char *service_name)
{
    char line[HYBBX_LINE_MAX];

    if (version == NULL) {
        version = HYBBX_VERSION_STRING;
    }
    if (service_name == NULL || service_name[0] == '\0') {
        service_name = HYBBX_DEFAULT_SERVICE_NAME;
    }

    snprintf(line, sizeof(line), "HyBBX %s", version);
    hybbx_session_write_line(session, line);
    snprintf(line, sizeof(line), "Online at %s", service_name);
    hybbx_session_write_line(session, line);
    return HYBBX_OK;
}

hybbx_result_t hybbx_texts_send_banner(const hybbx_texts_config_t *texts,
                                       hybbx_session_t *session,
                                       const char *version,
                                       const char *service_name)
{
    char path[HYBBX_PATH_MAX];
    char expanded[HYBBX_LINE_MAX];
    FILE *fp;
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (texts == NULL || session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_texts_resolve(texts, HYBBX_TEXT_BANNER, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return send_banner_fallback(session, version, service_name);
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return send_banner_fallback(session, version, service_name);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n;

        expand_banner_line(expanded, sizeof(expanded), line, version,
                          service_name);
        n = strlen(expanded);
        while (n > 0 && (expanded[n - 1] == '\n' || expanded[n - 1] == '\r')) {
            expanded[--n] = '\0';
        }

        if (expanded[0] != '\0') {
            hybbx_session_write_line(session, expanded);
        } else {
            hybbx_session_write(session, "\n");
        }
    }

    fclose(fp);
    return HYBBX_OK;
}

hybbx_result_t hybbx_texts_send_motd(const hybbx_texts_config_t *texts,
                                     hybbx_session_t *session)
{
    char path[HYBBX_PATH_MAX];
    char expanded[HYBBX_LINE_MAX];
    FILE *fp;
    char line[HYBBX_LINE_MAX];
    const char *username;
    hybbx_result_t rc;

    if (texts == NULL || session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    username = hybbx_session_display_name(session);
    if (username[0] == '\0') {
        username = "visitor";
    }

    rc = hybbx_texts_resolve(texts, HYBBX_TEXT_MOTD, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n;

        expand_text_line(expanded, sizeof(expanded), line, NULL, NULL, username);
        n = strlen(expanded);
        while (n > 0 && (expanded[n - 1] == '\n' || expanded[n - 1] == '\r')) {
            expanded[--n] = '\0';
        }

        if (expanded[0] != '\0') {
            hybbx_session_write_line(session, expanded);
        } else {
            hybbx_session_write(session, "\n");
        }
    }

    fclose(fp);
    return HYBBX_OK;
}

hybbx_result_t hybbx_texts_send_file(const hybbx_texts_config_t *texts,
                                     hybbx_session_t *session,
                                     const char *filename)
{
    char path[HYBBX_PATH_MAX];
    FILE *fp;
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (texts == NULL || session == NULL || filename == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_texts_resolve(texts, filename, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }

        if (line[0] != '\0') {
            hybbx_session_write_line(session, line);
        } else {
            hybbx_session_write(session, "\n");
        }
    }

    fclose(fp);
    return HYBBX_OK;
}
