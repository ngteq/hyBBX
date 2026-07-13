#include "hybbx/commands_registry.h"
#include "hybbx/session.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct menu_subarea {
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned verb_count;
} menu_subarea_t;

typedef struct menu_area {
    char label[16];
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned verb_count;
    menu_subarea_t subareas[HYBBX_AREAS_SUB_MAX];
    unsigned subarea_count;
} menu_area_t;

typedef struct alias_entry {
    char canonical[HYBBX_CMD_VERB_MAX];
    char aliases[HYBBX_COMMANDS_ALIAS_PER][HYBBX_CMD_VERB_MAX];
    unsigned alias_count;
} alias_entry_t;

typedef struct target_right_rule {
    hybbx_user_level_t actor;
    hybbx_user_level_t targets[5];
    unsigned target_count;
} target_right_rule_t;

typedef struct promote_right_rule {
    hybbx_user_level_t actor;
    hybbx_user_level_t to_level;
    hybbx_user_level_t from_levels[4];
    unsigned from_count;
} promote_right_rule_t;

typedef struct demote_right_rule {
    hybbx_user_level_t actor;
    hybbx_user_level_t from_levels[4];
    unsigned from_count;
} demote_right_rule_t;

typedef struct commands_registry {
    int loaded;
    char menu_header[HYBBX_COMMANDS_HEADER_MAX];
    char index_header[HYBBX_COMMANDS_HEADER_MAX];
    char alias_header[HYBBX_COMMANDS_HEADER_MAX];
    menu_area_t areas[HYBBX_AREAS_MAX];
    unsigned area_count;
    char menu_levels[HYBBX_MENU_LEVELS_MAX][16];
    char menu_area_labels[HYBBX_MENU_LEVELS_MAX]
                         [HYBBX_MENU_AREAS_PER_LEVEL][16];
    unsigned menu_area_count[HYBBX_MENU_LEVELS_MAX];
    unsigned menu_level_count;
    char index_labels[HYBBX_MENU_AREAS_PER_LEVEL][16];
    unsigned index_label_count;
    target_right_rule_t userchange_rules[HYBBX_RIGHTS_TARGET_RULES_MAX];
    unsigned userchange_rule_count;
    target_right_rule_t userdelete_rules[HYBBX_RIGHTS_TARGET_RULES_MAX];
    unsigned userdelete_rule_count;
    target_right_rule_t delete_rules[HYBBX_RIGHTS_TARGET_RULES_MAX];
    unsigned delete_rule_count;
    promote_right_rule_t promote_rules[HYBBX_RIGHTS_PROMOTE_RULES_MAX];
    unsigned promote_rule_count;
    demote_right_rule_t demote_rules[HYBBX_RIGHTS_DEMOTE_RULES_MAX];
    unsigned demote_rule_count;
    alias_entry_t aliases[HYBBX_COMMANDS_ALIASES_MAX];
    unsigned alias_count;
    hybbx_command_def_t commands[HYBBX_COMMANDS_MAX];
    unsigned command_count;
    char alias_lines[HYBBX_COMMANDS_ALIAS_LINES][HYBBX_LINE_MAX];
    unsigned alias_line_count;
} commands_registry_t;

static commands_registry_t g_registry;

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

static void trim_inplace(char *s)
{
    char *start = s;
    char *end;

    if (s == NULL) {
        return;
    }

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
}

static void strip_quotes(char *s)
{
    size_t len;

    if (s == NULL) {
        return;
    }

    trim_inplace(s);
    len = strlen(s);
    if (len >= 2 &&
        ((s[0] == '"' && s[len - 1] == '"') ||
         (s[0] == '\'' && s[len - 1] == '\''))) {
        s[len - 1] = '\0';
        memmove(s, s + 1, len - 1);
    }
}

static unsigned line_indent(const char *line)
{
    unsigned n = 0;

    while (line[n] == ' ') {
        n++;
    }

    return n;
}

static const char *line_key(const char *line)
{
    if (line == NULL) {
        return "";
    }

    while (*line == ' ') {
        line++;
    }

    return line;
}

static int parse_bracket_list(const char *value, char out[][HYBBX_CMD_VERB_MAX],
                              unsigned max, unsigned *count)
{
    const char *p;
    char token[HYBBX_CMD_VERB_MAX];
    size_t tlen;

    if (value == NULL || count == NULL) {
        return 0;
    }

    *count = 0;
    p = strchr(value, '[');
    if (p == NULL) {
        return 0;
    }
    p++;

    while (*p != '\0' && *p != ']') {
        while (*p == ' ' || *p == ',') {
            p++;
        }
        if (*p == ']' || *p == '\0') {
            break;
        }

        tlen = 0;
        while (*p != '\0' && *p != ',' && *p != ']' &&
               !isspace((unsigned char)*p) && tlen + 1 < sizeof(token)) {
            token[tlen++] = *p++;
        }
        token[tlen] = '\0';
        if (token[0] == '\0') {
            continue;
        }

        if (*count >= max) {
            return -1;
        }

        hybbx_strlcpy(out[*count], token, HYBBX_CMD_VERB_MAX);
        (*count)++;
    }

    return 0;
}

static int parse_level_list(const char *value, hybbx_user_level_t *out,
                            unsigned max, unsigned *count)
{
    char tokens[8][HYBBX_CMD_VERB_MAX];
    unsigned n = 0;
    unsigned i;

    if (value == NULL || count == NULL) {
        return 0;
    }

    if (parse_bracket_list(value, tokens, max, &n) != 0) {
        return -1;
    }

    *count = 0;
    for (i = 0; i < n && *count < max; i++) {
        out[*count] = hybbx_user_level_parse(tokens[i]);
        (*count)++;
    }

    return 0;
}

static hybbx_user_level_t level_from_name(const char *name)
{
    return hybbx_user_level_parse(name);
}

static const char *menu_level_name(hybbx_user_level_t level)
{
    switch (level) {
    case HYBBX_LEVEL_SYSOP:
        return "Sysop";
    case HYBBX_LEVEL_ADMIN:
        return "Admin";
    case HYBBX_LEVEL_MOD:
        return "Mod";
    case HYBBX_LEVEL_USER:
        return "User";
    default:
        return "Guest";
    }
}

static const menu_area_t *area_find(const char *label)
{
    unsigned i;

    if (label == NULL) {
        return NULL;
    }

    for (i = 0; i < g_registry.area_count; i++) {
        if (str_ieq(g_registry.areas[i].label, label)) {
            return &g_registry.areas[i];
        }
    }

    return NULL;
}

static int level_in_list(hybbx_user_level_t level,
                         const hybbx_user_level_t *list,
                         unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
        if (list[i] == level) {
            return 1;
        }
    }

    return 0;
}

static int command_level_allowed(const hybbx_command_def_t *def,
                                 hybbx_user_level_t level)
{
    if (def == NULL) {
        return 0;
    }

    if (def->only_level != 0 && level != (hybbx_user_level_t)def->only_level) {
        return 0;
    }

    if (level > def->min_level) {
        return 0;
    }

    if (def->max_level != 0 && level < def->max_level) {
        return 0;
    }

    return 1;
}

static int target_right_allowed(const target_right_rule_t *rules,
                                unsigned rule_count,
                                hybbx_user_level_t actor,
                                hybbx_user_level_t target)
{
    unsigned i;

    if (hybbx_user_level_is_sysop(target) ||
        hybbx_user_level_is_guest(target)) {
        return 0;
    }

    for (i = 0; i < rule_count; i++) {
        if (rules[i].actor == actor &&
            level_in_list(target, rules[i].targets, rules[i].target_count)) {
            return 1;
        }
    }

    return 0;
}

static hybbx_result_t parse_areas_yaml(commands_registry_t *reg, const char *path)
{
    FILE *fp;
    char buf[HYBBX_CONFIG_LINE_MAX];
    enum {
        SEC_NONE,
        SEC_META,
        SEC_AREAS,
        SEC_MENU,
        SEC_INDEX
    } section = SEC_NONE;
    menu_area_t *area = NULL;
    menu_subarea_t *subarea = NULL;
    char cur_menu_level[16];

    if (reg == NULL || path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    cur_menu_level[0] = '\0';

    fp = fopen(path, "r");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *line = buf;
        char *colon;
        unsigned indent;

        indent = line_indent(line);
        trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (indent == 0 && line[strlen(line) - 1] == ':') {
            line[strlen(line) - 1] = '\0';
            area = NULL;
            subarea = NULL;
            cur_menu_level[0] = '\0';

            if (str_ieq(line, "meta")) {
                section = SEC_META;
            } else if (str_ieq(line, "areas")) {
                section = SEC_AREAS;
            } else if (str_ieq(line, "menu")) {
                section = SEC_MENU;
            } else if (str_ieq(line, "index")) {
                section = SEC_INDEX;
            } else {
                section = SEC_NONE;
            }
            continue;
        }

        if (section == SEC_META && indent >= 2) {
            colon = strchr(line, ':');
            if (colon == NULL) {
                continue;
            }
            *colon = '\0';
            strip_quotes(colon + 1);
            if (str_ieq(line_key(line), "menu_header")) {
                hybbx_strlcpy(reg->menu_header, colon + 1,
                              sizeof(reg->menu_header));
            } else if (str_ieq(line_key(line), "index_header")) {
                hybbx_strlcpy(reg->index_header, colon + 1,
                              sizeof(reg->index_header));
            } else if (str_ieq(line_key(line), "alias_header")) {
                hybbx_strlcpy(reg->alias_header, colon + 1,
                              sizeof(reg->alias_header));
            }
            continue;
        }

        if (section == SEC_AREAS) {
            if (indent == 2 && line[0] == '-') {
                if (reg->area_count >= HYBBX_AREAS_MAX) {
                    continue;
                }
                area = &reg->areas[reg->area_count++];
                memset(area, 0, sizeof(*area));
                subarea = NULL;

                line++;
                trim_inplace(line);
                colon = strchr(line, ':');
                if (colon != NULL) {
                    *colon = '\0';
                    strip_quotes(colon + 1);
                    if (str_ieq(line_key(line), "label")) {
                        hybbx_strlcpy(area->label, colon + 1, sizeof(area->label));
                    } else if (str_ieq(line_key(line), "commands")) {
                        parse_bracket_list(colon + 1, area->verbs,
                                           HYBBX_COMMANDS_VERBS_PER_GROUP,
                                           &area->verb_count);
                    }
                }
                continue;
            }

            if (area == NULL) {
                continue;
            }

            if (indent == 4 && line[0] == '-') {
                if (area->subarea_count < HYBBX_AREAS_SUB_MAX) {
                    subarea = &area->subareas[area->subarea_count++];
                    memset(subarea, 0, sizeof(*subarea));
                }
                continue;
            }

            if (indent >= 4) {
                colon = strchr(line, ':');
                if (colon == NULL) {
                    continue;
                }
                *colon = '\0';
                strip_quotes(colon + 1);
                if (str_ieq(line_key(line), "label")) {
                    hybbx_strlcpy(area->label, colon + 1, sizeof(area->label));
                } else if (str_ieq(line_key(line), "commands")) {
                    parse_bracket_list(colon + 1, area->verbs,
                                       HYBBX_COMMANDS_VERBS_PER_GROUP,
                                       &area->verb_count);
                }
                continue;
            }

            if (indent >= 6 && subarea != NULL) {
                colon = strchr(line, ':');
                if (colon == NULL) {
                    continue;
                }
                *colon = '\0';
                strip_quotes(colon + 1);
                if (str_ieq(line_key(line), "commands")) {
                    parse_bracket_list(colon + 1, subarea->verbs,
                                       HYBBX_COMMANDS_VERBS_PER_GROUP,
                                       &subarea->verb_count);
                }
            }
            continue;
        }

        if (section == SEC_MENU) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                if (reg->menu_level_count < HYBBX_MENU_LEVELS_MAX) {
                    hybbx_strlcpy(reg->menu_levels[reg->menu_level_count],
                                  line_key(line),
                                  sizeof(reg->menu_levels[0]));
                    hybbx_strlcpy(cur_menu_level, line_key(line),
                                  sizeof(cur_menu_level));
                    reg->menu_level_count++;
                }
                continue;
            }

            if (indent >= 4 && line[0] == '-' && cur_menu_level[0] != '\0') {
                unsigned i;

                for (i = 0; i < reg->menu_level_count; i++) {
                    unsigned n;

                    if (!str_ieq(reg->menu_levels[i], cur_menu_level)) {
                        continue;
                    }

                    n = reg->menu_area_count[i];
                    if (n >= HYBBX_MENU_AREAS_PER_LEVEL) {
                        break;
                    }

                    line++;
                    trim_inplace(line);
                    hybbx_strlcpy(reg->menu_area_labels[i][n], line, 16);
                    reg->menu_area_count[i]++;
                    break;
                }
            }
            continue;
        }

        if (section == SEC_INDEX && indent >= 2 && line[0] == '-') {
            if (reg->index_label_count < HYBBX_MENU_AREAS_PER_LEVEL) {
                line++;
                trim_inplace(line);
                hybbx_strlcpy(reg->index_labels[reg->index_label_count],
                              line, 16);
                reg->index_label_count++;
            }
        }
    }

    fclose(fp);

    if (reg->menu_header[0] == '\0' || reg->index_header[0] == '\0' ||
        reg->area_count == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (reg->index_label_count == 0) {
        unsigned i;

        for (i = 0; i < reg->area_count &&
                    reg->index_label_count < HYBBX_MENU_AREAS_PER_LEVEL;
             i++) {
            hybbx_strlcpy(reg->index_labels[reg->index_label_count],
                          reg->areas[i].label, 16);
            reg->index_label_count++;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t parse_commands_yaml(commands_registry_t *reg,
                                            const char *path)
{
    FILE *fp;
    char buf[HYBBX_CONFIG_LINE_MAX];
    enum {
        SEC_NONE,
        SEC_RIGHTS,
        SEC_ALIASES,
        SEC_COMMANDS
    } section = SEC_NONE;
    enum {
        RIGHT_NONE,
        RIGHT_USERCHANGE,
        RIGHT_USERDELETE,
        RIGHT_DELETE,
        RIGHT_PROMOTE,
        RIGHT_DEMOTE
    } right_kind = RIGHT_NONE;
    char cur_alias_canonical[HYBBX_CMD_VERB_MAX];
    hybbx_command_def_t *cmd = NULL;
    target_right_rule_t *target_rule = NULL;
    promote_right_rule_t *promote_rule = NULL;
    demote_right_rule_t *demote_rule = NULL;

    if (reg == NULL || path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    cur_alias_canonical[0] = '\0';

    fp = fopen(path, "r");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *line = buf;
        char *colon;
        unsigned indent;

        indent = line_indent(line);
        trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (indent == 0 && line[strlen(line) - 1] == ':') {
            line[strlen(line) - 1] = '\0';
            cmd = NULL;
            target_rule = NULL;
            promote_rule = NULL;
            demote_rule = NULL;

            if (str_ieq(line, "rights")) {
                section = SEC_RIGHTS;
                right_kind = RIGHT_NONE;
            } else if (str_ieq(line, "aliases")) {
                section = SEC_ALIASES;
                right_kind = RIGHT_NONE;
            } else if (str_ieq(line, "commands")) {
                section = SEC_COMMANDS;
                right_kind = RIGHT_NONE;
            } else {
                section = SEC_NONE;
            }
            continue;
        }

        if (section == SEC_RIGHTS) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                target_rule = NULL;
                promote_rule = NULL;
                demote_rule = NULL;

                if (str_ieq(line_key(line), "userchange")) {
                    right_kind = RIGHT_USERCHANGE;
                } else if (str_ieq(line_key(line), "userdelete")) {
                    right_kind = RIGHT_USERDELETE;
                } else if (str_ieq(line_key(line), "delete")) {
                    right_kind = RIGHT_DELETE;
                } else if (str_ieq(line_key(line), "promote")) {
                    right_kind = RIGHT_PROMOTE;
                } else if (str_ieq(line_key(line), "demote")) {
                    right_kind = RIGHT_DEMOTE;
                } else {
                    right_kind = RIGHT_NONE;
                }
                continue;
            }

            if (indent == 4 && line[0] == '-') {
                if (right_kind == RIGHT_USERCHANGE &&
                    reg->userchange_rule_count < HYBBX_RIGHTS_TARGET_RULES_MAX) {
                    target_rule =
                        &reg->userchange_rules[reg->userchange_rule_count++];
                    memset(target_rule, 0, sizeof(*target_rule));
                } else if (right_kind == RIGHT_USERDELETE &&
                           reg->userdelete_rule_count <
                               HYBBX_RIGHTS_TARGET_RULES_MAX) {
                    target_rule =
                        &reg->userdelete_rules[reg->userdelete_rule_count++];
                    memset(target_rule, 0, sizeof(*target_rule));
                } else if (right_kind == RIGHT_DELETE &&
                           reg->delete_rule_count <
                               HYBBX_RIGHTS_TARGET_RULES_MAX) {
                    target_rule =
                        &reg->delete_rules[reg->delete_rule_count++];
                    memset(target_rule, 0, sizeof(*target_rule));
                } else if (right_kind == RIGHT_PROMOTE &&
                           reg->promote_rule_count <
                               HYBBX_RIGHTS_PROMOTE_RULES_MAX) {
                    promote_rule =
                        &reg->promote_rules[reg->promote_rule_count++];
                    memset(promote_rule, 0, sizeof(*promote_rule));
                } else if (right_kind == RIGHT_DEMOTE &&
                           reg->demote_rule_count <
                               HYBBX_RIGHTS_DEMOTE_RULES_MAX) {
                    demote_rule =
                        &reg->demote_rules[reg->demote_rule_count++];
                    memset(demote_rule, 0, sizeof(*demote_rule));
                }
                continue;
            }

            if (indent >= 6) {
                colon = strchr(line, ':');
                if (colon == NULL) {
                    continue;
                }
                *colon = '\0';
                strip_quotes(colon + 1);

                if (target_rule != NULL) {
                    if (str_ieq(line_key(line), "actor")) {
                        target_rule->actor = level_from_name(colon + 1);
                    } else if (str_ieq(line_key(line), "targets")) {
                        parse_level_list(colon + 1, target_rule->targets, 5,
                                         &target_rule->target_count);
                    }
                } else if (promote_rule != NULL) {
                    if (str_ieq(line_key(line), "actor")) {
                        promote_rule->actor = level_from_name(colon + 1);
                    } else if (str_ieq(line_key(line), "to")) {
                        promote_rule->to_level = level_from_name(colon + 1);
                    } else if (str_ieq(line_key(line), "from")) {
                        parse_level_list(colon + 1, promote_rule->from_levels,
                                         4, &promote_rule->from_count);
                    }
                } else if (demote_rule != NULL) {
                    if (str_ieq(line_key(line), "actor")) {
                        demote_rule->actor = level_from_name(colon + 1);
                    } else if (str_ieq(line_key(line), "from")) {
                        parse_level_list(colon + 1, demote_rule->from_levels,
                                         4, &demote_rule->from_count);
                    }
                }
            }
            continue;
        }

        if (section == SEC_ALIASES) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                alias_entry_t *entry;

                line[strlen(line) - 1] = '\0';
                if (reg->alias_count >= HYBBX_COMMANDS_ALIASES_MAX) {
                    continue;
                }
                entry = &reg->aliases[reg->alias_count++];
                hybbx_strlcpy(entry->canonical, line_key(line),
                              sizeof(entry->canonical));
                hybbx_strlcpy(cur_alias_canonical, line_key(line),
                              sizeof(cur_alias_canonical));
                continue;
            }
            if (indent >= 4 && line[0] == '-' && cur_alias_canonical[0] != '\0') {
                alias_entry_t *entry = &reg->aliases[reg->alias_count - 1];

                if (entry->alias_count < HYBBX_COMMANDS_ALIAS_PER) {
                    line++;
                    trim_inplace(line);
                    strip_quotes(line);
                    hybbx_strlcpy(entry->aliases[entry->alias_count], line,
                                  HYBBX_CMD_VERB_MAX);
                    entry->alias_count++;
                }
            }
            continue;
        }

        if (section == SEC_COMMANDS) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                if (reg->command_count < HYBBX_COMMANDS_MAX) {
                    cmd = &reg->commands[reg->command_count++];
                    memset(cmd, 0, sizeof(*cmd));
                    cmd->min_level = HYBBX_LEVEL_GUEST;
                    hybbx_strlcpy(cmd->verb, line_key(line), sizeof(cmd->verb));
                }
                continue;
            }
            if (indent >= 4 && cmd != NULL) {
                colon = strchr(line, ':');
                if (colon == NULL) {
                    continue;
                }
                *colon = '\0';
                strip_quotes(colon + 1);
                if (str_ieq(line_key(line), "group")) {
                    hybbx_strlcpy(cmd->group, colon + 1, sizeof(cmd->group));
                } else if (str_ieq(line_key(line), "min")) {
                    cmd->min_level = level_from_name(colon + 1);
                } else if (str_ieq(line_key(line), "max")) {
                    cmd->max_level = level_from_name(colon + 1);
                } else if (str_ieq(line_key(line), "only")) {
                    cmd->only_level = (int)level_from_name(colon + 1);
                } else if (str_ieq(line_key(line), "line1")) {
                    hybbx_strlcpy(cmd->line1, colon + 1, sizeof(cmd->line1));
                } else if (str_ieq(line_key(line), "line2")) {
                    hybbx_strlcpy(cmd->line2, colon + 1, sizeof(cmd->line2));
                }
            }
        }
    }

    fclose(fp);

    if (reg->command_count == 0) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

static void build_alias_lines(commands_registry_t *reg)
{
    char line[HYBBX_LINE_MAX];
    unsigned i;
    unsigned j;
    unsigned off;

    reg->alias_line_count = 0;

    for (i = 0; i < reg->alias_count && reg->alias_line_count < HYBBX_COMMANDS_ALIAS_LINES;
         i++) {
        const alias_entry_t *entry = &reg->aliases[i];

        off = 0;
        line[0] = '\0';

        for (j = 0; j < entry->alias_count; j++) {
            const char *alias = entry->aliases[j];
            int n;

            if (strchr(alias, ' ') != NULL) {
                n = snprintf(line + off, sizeof(line) - off, "%s%s",
                             off > 0 ? "   " : "", alias);
            } else {
                n = snprintf(line + off, sizeof(line) - off, "%s%s -> %s",
                             off > 0 ? "   " : "", alias, entry->canonical);
            }

            if (n < 0 || (size_t)n >= sizeof(line) - off) {
                if (reg->alias_line_count < HYBBX_COMMANDS_ALIAS_LINES) {
                    hybbx_strlcpy(reg->alias_lines[reg->alias_line_count++],
                                  line, sizeof(reg->alias_lines[0]));
                }
                off = 0;
                line[0] = '\0';
                j--;
                continue;
            }
            off += (unsigned)n;
        }

        if (off > 0 && reg->alias_line_count < HYBBX_COMMANDS_ALIAS_LINES) {
            hybbx_strlcpy(reg->alias_lines[reg->alias_line_count++],
                          line, sizeof(reg->alias_lines[0]));
        }
    }
}

static hybbx_result_t load_yaml_file(const char *rel_share,
                                     hybbx_result_t (*parse_fn)(commands_registry_t *,
                                                                const char *),
                                     const char *label)
{
    char path[HYBBX_PATH_MAX];
    hybbx_result_t rc;

    if (hybbx_path_resolve(path, sizeof(path), rel_share) == HYBBX_OK) {
        rc = parse_fn(&g_registry, path);
        if (rc == HYBBX_OK) {
            hybbx_log_info("[commands] loaded %s (%s)", path, label);
            return HYBBX_OK;
        }
    }

    return HYBBX_ERR_IO;
}

hybbx_result_t hybbx_commands_registry_init(void)
{
    hybbx_result_t rc;

    hybbx_commands_registry_shutdown();

    rc = load_yaml_file(HYBBX_FILE_AREAS, parse_areas_yaml, "areas");
    if (rc != HYBBX_OK) {
        rc = load_yaml_file("share/areas.yaml", parse_areas_yaml, "areas");
    }
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[commands] failed to load " HYBBX_FILE_AREAS);
        return HYBBX_ERR_IO;
    }

    rc = load_yaml_file(HYBBX_FILE_COMMANDS, parse_commands_yaml, "commands");
    if (rc != HYBBX_OK) {
        rc = load_yaml_file("share/commands.yaml", parse_commands_yaml, "commands");
    }
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[commands] failed to load " HYBBX_FILE_COMMANDS);
        hybbx_commands_registry_shutdown();
        return HYBBX_ERR_IO;
    }

    build_alias_lines(&g_registry);
    g_registry.loaded = 1;
    hybbx_log_info("[commands] registry ready (%u areas, %u verbs)",
                   g_registry.area_count, g_registry.command_count);
    return HYBBX_OK;
}

void hybbx_commands_registry_shutdown(void)
{
    memset(&g_registry, 0, sizeof(g_registry));
}

const hybbx_command_def_t *hybbx_commands_registry_find(const char *verb)
{
    unsigned i;

    if (!g_registry.loaded || verb == NULL) {
        return NULL;
    }

    for (i = 0; i < g_registry.command_count; i++) {
        if (str_ieq(g_registry.commands[i].verb, verb)) {
            return &g_registry.commands[i];
        }
    }

    return NULL;
}

const char *hybbx_commands_registry_canonical(const char *topic)
{
    unsigned i;
    unsigned j;

    if (!g_registry.loaded || topic == NULL || topic[0] == '\0') {
        return topic;
    }

    if (hybbx_commands_registry_find(topic) != NULL) {
        return topic;
    }

    for (i = 0; i < g_registry.alias_count; i++) {
        for (j = 0; j < g_registry.aliases[i].alias_count; j++) {
            const char *alias = g_registry.aliases[i].aliases[j];

            if (strchr(alias, ' ') != NULL) {
                continue;
            }
            if (str_ieq(alias, topic)) {
                return g_registry.aliases[i].canonical;
            }
        }
    }

    return topic;
}

int hybbx_commands_registry_verb_allowed(hybbx_user_level_t level,
                                         const char *verb)
{
    const char *canonical;
    const hybbx_command_def_t *def;

    if (verb == NULL || verb[0] == '\0') {
        return 1;
    }

    if (!g_registry.loaded) {
        return 0;
    }

    canonical = hybbx_commands_registry_canonical(verb);
    def = hybbx_commands_registry_find(canonical);
    if (def == NULL) {
        return 0;
    }

    return command_level_allowed(def, level);
}

int hybbx_commands_registry_help_allowed(hybbx_user_level_t level,
                                         const char *verb)
{
    return hybbx_commands_registry_verb_allowed(level, verb);
}

int hybbx_commands_registry_may_userchange(hybbx_user_level_t actor,
                                           hybbx_user_level_t target)
{
    if (!g_registry.loaded) {
        return 0;
    }

    return target_right_allowed(g_registry.userchange_rules,
                                g_registry.userchange_rule_count,
                                actor, target);
}

int hybbx_commands_registry_may_userdelete(hybbx_user_level_t actor,
                                           hybbx_user_level_t target)
{
    unsigned i;

    if (!g_registry.loaded || hybbx_user_level_is_sysop(target)) {
        return 0;
    }

    for (i = 0; i < g_registry.userdelete_rule_count; i++) {
        const target_right_rule_t *rule = &g_registry.userdelete_rules[i];

        if (rule->actor == actor &&
            level_in_list(target, rule->targets, rule->target_count)) {
            return 1;
        }
    }

    return 0;
}

int hybbx_commands_registry_may_delete(hybbx_user_level_t actor,
                                       hybbx_user_level_t target)
{
    if (!g_registry.loaded) {
        return 0;
    }

    if (target == HYBBX_LEVEL_SYSOP) {
        return 0;
    }

    return target_right_allowed(g_registry.delete_rules,
                                g_registry.delete_rule_count,
                                actor, target);
}

int hybbx_commands_registry_may_promote(hybbx_user_level_t actor,
                                        hybbx_user_level_t target,
                                        int target_active,
                                        hybbx_user_level_t new_level)
{
    unsigned i;

    if (!g_registry.loaded || !target_active ||
        hybbx_user_level_is_guest(target) ||
        target == HYBBX_LEVEL_SYSOP) {
        return 0;
    }

    for (i = 0; i < g_registry.promote_rule_count; i++) {
        const promote_right_rule_t *rule = &g_registry.promote_rules[i];

        if (rule->actor == actor && rule->to_level == new_level &&
            level_in_list(target, rule->from_levels, rule->from_count)) {
            return 1;
        }
    }

    return 0;
}

int hybbx_commands_registry_may_demote(hybbx_user_level_t actor,
                                         hybbx_user_level_t target)
{
    unsigned i;

    if (!g_registry.loaded) {
        return 0;
    }

    for (i = 0; i < g_registry.demote_rule_count; i++) {
        const demote_right_rule_t *rule = &g_registry.demote_rules[i];

        if (rule->actor == actor &&
            level_in_list(target, rule->from_levels, rule->from_count)) {
            return 1;
        }
    }

    return 0;
}

static void emit_menu_line(hybbx_session_t *session, const char *label,
                           const char *cmds, int continuation)
{
    char buf[HYBBX_LINE_MAX];

    if (continuation) {
        snprintf(buf, sizeof(buf), "             %s", cmds);
    } else {
        snprintf(buf, sizeof(buf), "  %-10s %s", label, cmds);
    }
    hybbx_session_write_line(session, buf);
}

static void format_verbs_line(char *out, size_t out_len,
                              const char verbs[][HYBBX_CMD_VERB_MAX],
                              unsigned count)
{
    size_t off = 0;
    unsigned i;

    out[0] = '\0';
    for (i = 0; i < count; i++) {
        int n;

        if (verbs[i][0] == '\0') {
            continue;
        }
        n = snprintf(out + off, out_len - off, "%s/%s",
                     off > 0 ? " " : "", verbs[i]);
        if (n < 0 || (size_t)n >= out_len - off) {
            break;
        }
        off += (size_t)n;
    }
}

static unsigned collect_area_verbs(const menu_area_t *area,
                                   hybbx_user_level_t level,
                                   int filter_access,
                                   char out[][HYBBX_CMD_VERB_MAX],
                                   unsigned max)
{
    unsigned count = 0;
    unsigned i;
    unsigned j;

    if (area == NULL) {
        return 0;
    }

    for (i = 0; i < area->verb_count && count < max; i++) {
        if (filter_access &&
            !hybbx_commands_registry_verb_allowed(level, area->verbs[i])) {
            continue;
        }
        hybbx_strlcpy(out[count], area->verbs[i], HYBBX_CMD_VERB_MAX);
        count++;
    }

    for (i = 0; i < area->subarea_count; i++) {
        const menu_subarea_t *sub = &area->subareas[i];

        for (j = 0; j < sub->verb_count && count < max; j++) {
            if (filter_access &&
                !hybbx_commands_registry_verb_allowed(level, sub->verbs[j])) {
                continue;
            }
            hybbx_strlcpy(out[count], sub->verbs[j], HYBBX_CMD_VERB_MAX);
            count++;
        }
    }

    return count;
}

static void render_area(hybbx_session_t *session, const menu_area_t *area,
                        hybbx_user_level_t level, int filter_access)
{
    char cmds[HYBBX_LINE_MAX];
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned count;
    unsigned i;

    if (area == NULL) {
        return;
    }

    count = collect_area_verbs(area, level, filter_access, verbs,
                               HYBBX_COMMANDS_VERBS_PER_GROUP);
    if (count == 0) {
        return;
    }

    format_verbs_line(cmds, sizeof(cmds),
                      (const char (*)[HYBBX_CMD_VERB_MAX])verbs, count);
    emit_menu_line(session, area->label, cmds, 0);

    if (!filter_access) {
        for (i = 0; i < area->subarea_count; i++) {
            const menu_subarea_t *sub = &area->subareas[i];
            char sub_verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
            unsigned sub_count = 0;
            unsigned j;

            for (j = 0; j < sub->verb_count; j++) {
                hybbx_strlcpy(sub_verbs[sub_count], sub->verbs[j],
                              HYBBX_CMD_VERB_MAX);
                sub_count++;
            }

            if (sub_count == 0) {
                continue;
            }

            format_verbs_line(cmds, sizeof(cmds),
                              (const char (*)[HYBBX_CMD_VERB_MAX])sub_verbs,
                              sub_count);
            emit_menu_line(session, "", cmds, 1);
        }
    }
}

static void render_area_labels(hybbx_session_t *session,
                               const char labels[][16],
                               unsigned label_count,
                               hybbx_user_level_t level,
                               int filter_access)
{
    unsigned i;

    for (i = 0; i < label_count; i++) {
        const menu_area_t *area = area_find(labels[i]);

        if (area != NULL) {
            render_area(session, area, level, filter_access);
        }
    }
}

static unsigned menu_layout_index(hybbx_user_level_t level)
{
    const char *level_name = menu_level_name(level);
    unsigned i;

    for (i = 0; i < g_registry.menu_level_count; i++) {
        if (str_ieq(g_registry.menu_levels[i], level_name)) {
            return i;
        }
    }

    return g_registry.menu_level_count;
}

void hybbx_commands_registry_show_menu(hybbx_session_t *session)
{
    hybbx_user_level_t level;
    unsigned layout;

    if (session == NULL || !g_registry.loaded) {
        return;
    }

    level = hybbx_session_user_level(session);
    layout = menu_layout_index(level);

    hybbx_session_write_line(session, g_registry.menu_header);
    if (layout < g_registry.menu_level_count) {
        render_area_labels(session,
                           (const char (*)[16])
                               g_registry.menu_area_labels[layout],
                           g_registry.menu_area_count[layout],
                           level, 1);
    }
}

void hybbx_commands_registry_show_index(hybbx_session_t *session)
{
    if (session == NULL || !g_registry.loaded) {
        return;
    }

    hybbx_session_write_line(session, g_registry.index_header);
    render_area_labels(session,
                       (const char (*)[16])g_registry.index_labels,
                       g_registry.index_label_count,
                       HYBBX_LEVEL_GUEST, 0);
}

void hybbx_commands_registry_show_aliases(hybbx_session_t *session)
{
    unsigned i;

    if (session == NULL || !g_registry.loaded) {
        return;
    }

    hybbx_session_write_line(session, g_registry.alias_header);
    for (i = 0; i < g_registry.alias_line_count; i++) {
        hybbx_session_write_line(session, g_registry.alias_lines[i]);
    }
}

void hybbx_commands_registry_show_help(hybbx_session_t *session,
                                       const char *canonical)
{
    const hybbx_command_def_t *def;

    if (session == NULL || canonical == NULL || !g_registry.loaded) {
        return;
    }

    def = hybbx_commands_registry_find(canonical);
    if (def == NULL || def->line1[0] == '\0') {
        return;
    }

    hybbx_session_write_line(session, def->line1);
    if (def->line2[0] != '\0') {
        hybbx_session_write_line(session, def->line2);
    }
}
