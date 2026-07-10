#ifndef HYBBX_COMMAND_H
#define HYBBX_COMMAND_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

/**
 * Input line routing scope.
 *
 * HyBBX accepts only lines starting with '/'. Valid forms:
 *   /              or /command /cmd /commands → /help
 *   /<verb>        or / <verb>              → command
 *   /command <verb> or /cmd <verb>          → same as / <verb>
 *
 * - COMMENT: starts with ';' or '#' — ignored (like empty line)
 * - LOCAL: anything else — not HyBBX; silently ignored
 */
typedef enum hybbx_command_scope {
    HYBBX_CMD_SCOPE_HYBBX = 1,
    HYBBX_CMD_SCOPE_LOCAL = 2,
    HYBBX_CMD_SCOPE_COMMENT = 3
} hybbx_command_scope_t;

typedef struct hybbx_parsed_command {
    hybbx_command_scope_t scope;
    char *line;
    char *verb;
    char **argv;
    size_t argc;
} hybbx_parsed_command_t;

/**
 * Classify a single input line.
 * Returns HYBBX_CMD_SCOPE_HYBBX only when the first non-whitespace
 * character is '/'.
 */
hybbx_command_scope_t hybbx_command_classify(const char *line);

/** Non-zero when @p line is a HyBBX system command (starts with '/'). */
int hybbx_command_is_hybbx(const char *line);

/** Non-zero when @p line is a local comment (';' or '#' after whitespace). */
int hybbx_command_is_comment(const char *line);

/**
 * Parse a trimmed command line into verb and arguments.
 * For HyBBX commands the leading '/' is stripped from the verb.
 */
hybbx_result_t hybbx_command_parse(const char *line,
                                   hybbx_parsed_command_t *out);

void hybbx_command_free(hybbx_parsed_command_t *cmd);

/**
 * Dispatch a parsed HyBBX command (scope must be HYBBX_CMD_SCOPE_HYBBX).
 */
hybbx_result_t hybbx_command_dispatch(struct hybbx_service *service,
                                      struct hybbx_session *session,
                                      const hybbx_parsed_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_COMMAND_H */
