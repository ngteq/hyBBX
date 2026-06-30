#include "hybbx/command.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define HYBBX_CMD_ALIAS "command"

static char *hybbx_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    if (!hybbx_size_ok(len)) {
        return NULL;
    }
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

static const char *skip_leading_space(const char *line)
{
    if (line == NULL) {
        return NULL;
    }

    while (*line != '\0' && isspace((unsigned char)*line)) {
        line++;
    }

    return line;
}

static char *ltrim_copy(char *s)
{
    char *start;

    if (s == NULL) {
        return NULL;
    }

    start = (char *)skip_leading_space(s);
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    return s;
}

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

static int strip_command_alias(char *rest)
{
    size_t alias_len = strlen(HYBBX_CMD_ALIAS);

    if (rest[0] == '\0' || str_ieq(rest, HYBBX_CMD_ALIAS)) {
        return 1;
    }

    if (strncmp(rest, HYBBX_CMD_ALIAS, alias_len) == 0 &&
        isspace((unsigned char)rest[alias_len])) {
        memmove(rest, rest + alias_len, strlen(rest + alias_len) + 1);
        ltrim_copy(rest);
        return rest[0] == '\0';
    }

    return 0;
}

hybbx_command_scope_t hybbx_command_classify(const char *line)
{
    const char *p = skip_leading_space(line);

    if (p == NULL || *p == '\0') {
        return HYBBX_CMD_SCOPE_LOCAL;
    }

    if (*p == '/') {
        return HYBBX_CMD_SCOPE_HYBBX;
    }

    if (*p == ';' || *p == '#') {
        return HYBBX_CMD_SCOPE_COMMENT;
    }

    return HYBBX_CMD_SCOPE_LOCAL;
}

int hybbx_command_is_comment(const char *line)
{
    return hybbx_command_classify(line) == HYBBX_CMD_SCOPE_COMMENT;
}

int hybbx_command_is_hybbx(const char *line)
{
    return hybbx_command_classify(line) == HYBBX_CMD_SCOPE_HYBBX;
}

static size_t count_tokens(const char *line)
{
    size_t count = 0;
    size_t i = 0;
    int in_token = 0;

    while (line[i] != '\0') {
        if (!isspace((unsigned char)line[i])) {
            if (!in_token) {
                count++;
                in_token = 1;
            }
        } else {
            in_token = 0;
        }
        i++;
    }

    return count;
}

static hybbx_result_t split_tokens(char *line, char **tokens, size_t max)
{
    size_t count = 0;
    size_t i = 0;

    while (line[i] != '\0' && count < max) {
        while (line[i] != '\0' && isspace((unsigned char)line[i])) {
            i++;
        }
        if (line[i] == '\0') {
            break;
        }
        tokens[count++] = line + i;
        while (line[i] != '\0' && !isspace((unsigned char)line[i])) {
            i++;
        }
        if (line[i] != '\0') {
            line[i] = '\0';
            i++;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t parse_hybbx_line(const char *line, hybbx_parsed_command_t *out)
{
    char *rest;
    char *work;
    size_t token_count;
    char **tokens;
    size_t i;
    hybbx_result_t rc;

    if (line[0] != '/') {
        return HYBBX_ERR_INVALID;
    }

    rest = hybbx_strdup(line + 1);
    if (rest == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    ltrim_copy(rest);

    if (strip_command_alias(rest)) {
        out->verb = hybbx_strdup("help");
        if (out->verb == NULL) {
            free(rest);
            return HYBBX_ERR_NOMEM;
        }
        free(rest);
        return HYBBX_OK;
    }

    token_count = count_tokens(rest);
    if (token_count == 0) {
        out->verb = hybbx_strdup("help");
        free(rest);
        if (out->verb == NULL) {
            return HYBBX_ERR_NOMEM;
        }
        return HYBBX_OK;
    }

    if (token_count > HYBBX_CMD_TOKEN_MAX) {
        free(rest);
        return HYBBX_ERR_INVALID;
    }

    work = hybbx_strdup(rest);
    free(rest);
    if (work == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    tokens = calloc(token_count, sizeof(*tokens));
    if (tokens == NULL) {
        free(work);
        return HYBBX_ERR_NOMEM;
    }

    rc = split_tokens(work, tokens, token_count);
    if (rc != HYBBX_OK) {
        free(tokens);
        free(work);
        return rc;
    }

    out->verb = hybbx_strdup(tokens[0]);
    if (out->verb == NULL) {
        free(tokens);
        free(work);
        return HYBBX_ERR_NOMEM;
    }

    out->argc = token_count > 1 ? token_count - 1 : 0;
    if (out->argc > 0) {
        out->argv = calloc(out->argc, sizeof(*out->argv));
        if (out->argv == NULL) {
            free(tokens);
            free(work);
            hybbx_command_free(out);
            return HYBBX_ERR_NOMEM;
        }

        for (i = 0; i < out->argc; i++) {
            out->argv[i] = hybbx_strdup(tokens[i + 1]);
            if (out->argv[i] == NULL) {
                free(tokens);
                free(work);
                hybbx_command_free(out);
                return HYBBX_ERR_NOMEM;
            }
        }
    }

    free(tokens);
    free(work);
    return HYBBX_OK;
}

hybbx_result_t hybbx_command_parse(const char *line,
                                   hybbx_parsed_command_t *out)
{
    char *trimmed;
    hybbx_result_t rc;

    if (line == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));

    trimmed = hybbx_strdup(skip_leading_space(line));
    if (trimmed == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    out->scope = hybbx_command_classify(trimmed);
    out->line = trimmed;

    if (out->scope != HYBBX_CMD_SCOPE_HYBBX) {
        return HYBBX_OK;
    }

    rc = parse_hybbx_line(trimmed, out);
    if (rc != HYBBX_OK) {
        hybbx_command_free(out);
    }

    return rc;
}

void hybbx_command_free(hybbx_parsed_command_t *cmd)
{
    size_t i;

    if (cmd == NULL) {
        return;
    }

    free(cmd->line);
    free(cmd->verb);

    for (i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);

    memset(cmd, 0, sizeof(*cmd));
}
