#ifndef HYBBX_COMMANDS_REGISTRY_H
#define HYBBX_COMMANDS_REGISTRY_H

#include "hybbx/auth.h"
#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_session;

typedef struct hybbx_command_def {
    char verb[HYBBX_CMD_VERB_MAX];
    char group[16];
    hybbx_user_level_t min_level;
    int only_level; /* HYBBX_LEVEL_* when set; else 0 */
    char line1[HYBBX_COMMANDS_HELP_LINE_MAX];
    char line2[HYBBX_COMMANDS_HELP_LINE_MAX];
} hybbx_command_def_t;

/** Load commands.yaml from install root. Call once at service startup. */
hybbx_result_t hybbx_commands_registry_init(void);

void hybbx_commands_registry_shutdown(void);

const hybbx_command_def_t *hybbx_commands_registry_find(const char *verb);

/** Map alias or topic name to canonical verb; returns @p topic if unknown. */
const char *hybbx_commands_registry_canonical(const char *topic);

int hybbx_commands_registry_verb_allowed(hybbx_user_level_t level,
                                         const char *verb);

void hybbx_commands_registry_show_menu(struct hybbx_session *session);
void hybbx_commands_registry_show_index(struct hybbx_session *session);
void hybbx_commands_registry_show_aliases(struct hybbx_session *session);
void hybbx_commands_registry_show_help(struct hybbx_session *session,
                                       const char *canonical);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_COMMANDS_REGISTRY_H */
