#include "hybbx/commands_registry.h"
#include "hybbx/session.h"
#include "hybbx/auth.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct cmd_group {
    char name[16];
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned count;
} cmd_group_t;

typedef struct menu_block {
    char label[16];
    char group[16];
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned verb_count;
    char omit[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned omit_count;
    char optional[4][HYBBX_CMD_VERB_MAX];
    unsigned optional_count;
    int continuation;
    int wrap;
} menu_block_t;

typedef struct menu_layout {
    char name[32];
    menu_block_t blocks[HYBBX_COMMANDS_MENU_BLOCKS];
    unsigned block_count;
} menu_layout_t;

typedef struct alias_entry {
    char canonical[HYBBX_CMD_VERB_MAX];
    char aliases[HYBBX_COMMANDS_ALIAS_PER][HYBBX_CMD_VERB_MAX];
    unsigned alias_count;
} alias_entry_t;

typedef struct commands_registry {
    int loaded;
    char menu_header[HYBBX_COMMANDS_HEADER_MAX];
    char index_header[HYBBX_COMMANDS_HEADER_MAX];
    char alias_header[HYBBX_COMMANDS_HEADER_MAX];
    cmd_group_t groups[HYBBX_COMMANDS_GROUP_MAX];
    unsigned group_count;
    menu_layout_t layouts[HYBBX_COMMANDS_MENU_LAYOUTS];
    unsigned layout_count;
    char menu_levels[HYBBX_COMMANDS_MENU_LEVELS][16];
    char menu_level_layouts[HYBBX_COMMANDS_MENU_LEVELS]
                           [HYBBX_COMMANDS_MENU_BLOCKS][32];
    unsigned menu_level_layout_count[HYBBX_COMMANDS_MENU_LEVELS];
    unsigned menu_level_count;
    char index_layouts[HYBBX_COMMANDS_MENU_BLOCKS][32];
    unsigned index_layout_count;
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

static hybbx_user_level_t level_from_name(const char *name)
{
    if (name == NULL) {
        return HYBBX_LEVEL_USER;
    }

    if (str_ieq(name, "Sysop")) {
        return HYBBX_LEVEL_SYSOP;
    }
    if (str_ieq(name, "Admin")) {
        return HYBBX_LEVEL_ADMIN;
    }
    if (str_ieq(name, "Mod")) {
        return HYBBX_LEVEL_MOD;
    }
    if (str_ieq(name, "User")) {
        return HYBBX_LEVEL_USER;
    }

    return HYBBX_LEVEL_GUEST;
}

static cmd_group_t *group_find(commands_registry_t *reg, const char *name)
{
    unsigned i;

    for (i = 0; i < reg->group_count; i++) {
        if (str_ieq(reg->groups[i].name, name)) {
            return &reg->groups[i];
        }
    }

    return NULL;
}

static menu_layout_t *layout_find(commands_registry_t *reg, const char *name)
{
    unsigned i;

    for (i = 0; i < reg->layout_count; i++) {
        if (str_ieq(reg->layouts[i].name, name)) {
            return &reg->layouts[i];
        }
    }

    return NULL;
}

static int verb_in_list(const char *verb,
                        const char list[][HYBBX_CMD_VERB_MAX],
                        unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
        if (str_ieq(list[i], verb)) {
            return 1;
        }
    }

    return 0;
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

static hybbx_result_t parse_commands_yaml(commands_registry_t *reg,
                                          const char *path)
{
    FILE *fp;
    char buf[HYBBX_CONFIG_LINE_MAX];
    enum {
        SEC_NONE,
        SEC_META,
        SEC_GROUPS,
        SEC_MENU,
        SEC_MENU_LAYOUTS,
        SEC_INDEX,
        SEC_ALIASES,
        SEC_COMMANDS
    } section = SEC_NONE;
    char cur_group[16];
    char cur_layout[32];
    char cur_command[HYBBX_CMD_VERB_MAX];
    char cur_alias_canonical[HYBBX_CMD_VERB_MAX];
    char cur_menu_level[16];
    menu_layout_t *layout = NULL;
    hybbx_command_def_t *cmd = NULL;
    unsigned i;

    if (reg == NULL || path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(reg, 0, sizeof(*reg));
    cur_group[0] = '\0';
    cur_layout[0] = '\0';
    cur_command[0] = '\0';
    cur_alias_canonical[0] = '\0';
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
            layout = NULL;
            cmd = NULL;

            if (str_ieq(line, "meta")) {
                section = SEC_META;
            } else if (str_ieq(line, "groups")) {
                section = SEC_GROUPS;
            } else if (str_ieq(line, "menu")) {
                section = SEC_MENU;
            } else if (str_ieq(line, "menu_layouts")) {
                section = SEC_MENU_LAYOUTS;
            } else if (str_ieq(line, "index")) {
                section = SEC_INDEX;
            } else if (str_ieq(line, "aliases")) {
                section = SEC_ALIASES;
            } else if (str_ieq(line, "commands")) {
                section = SEC_COMMANDS;
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

        if (section == SEC_GROUPS) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                if (reg->group_count < HYBBX_COMMANDS_GROUP_MAX) {
                    hybbx_strlcpy(reg->groups[reg->group_count].name,
                                  line_key(line),
                                  sizeof(reg->groups[0].name));
                    hybbx_strlcpy(cur_group, line_key(line), sizeof(cur_group));
                    reg->group_count++;
                }
                continue;
            }
            if (indent >= 4 && line[0] == '-' && cur_group[0] != '\0') {
                cmd_group_t *grp = group_find(reg, cur_group);

                if (grp == NULL || grp->count >= HYBBX_COMMANDS_VERBS_PER_GROUP) {
                    continue;
                }
                line++;
                trim_inplace(line);
                hybbx_strlcpy(grp->verbs[grp->count], line, HYBBX_CMD_VERB_MAX);
                grp->count++;
            }
            continue;
        }

        if (section == SEC_MENU_LAYOUTS) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                if (reg->layout_count < HYBBX_COMMANDS_MENU_LAYOUTS) {
                    hybbx_strlcpy(reg->layouts[reg->layout_count].name,
                                  line_key(line),
                                  sizeof(reg->layouts[0].name));
                    hybbx_strlcpy(cur_layout, line_key(line),
                                  sizeof(cur_layout));
                    layout = &reg->layouts[reg->layout_count];
                    reg->layout_count++;
                }
                continue;
            }

            if (layout == NULL) {
                continue;
            }

            if (indent == 4 && line[0] == '-') {
                if (layout->block_count < HYBBX_COMMANDS_MENU_BLOCKS) {
                    menu_block_t *block =
                        &layout->blocks[layout->block_count++];
                    char *payload = line + 1;

                    memset(block, 0, sizeof(*block));
                    trim_inplace(payload);
                    colon = strchr(payload, ':');
                    if (colon != NULL) {
                        *colon = '\0';
                        strip_quotes(colon + 1);
                        if (str_ieq(payload, "continuation")) {
                            block->continuation = 1;
                            parse_bracket_list(colon + 1, block->verbs,
                                               HYBBX_COMMANDS_VERBS_PER_GROUP,
                                               &block->verb_count);
                        } else if (str_ieq(payload, "label")) {
                            hybbx_strlcpy(block->label, colon + 1,
                                          sizeof(block->label));
                        }
                    }
                }
                continue;
            }

            if (indent >= 6 && layout->block_count > 0) {
                menu_block_t *block =
                    &layout->blocks[layout->block_count - 1];

                colon = strchr(line, ':');
                if (colon == NULL) {
                    continue;
                }
                *colon = '\0';
                strip_quotes(colon + 1);

                if (str_ieq(line_key(line), "label")) {
                    hybbx_strlcpy(block->label, colon + 1, sizeof(block->label));
                } else if (str_ieq(line_key(line), "group")) {
                    hybbx_strlcpy(block->group, colon + 1, sizeof(block->group));
                } else if (str_ieq(line_key(line), "wrap")) {
                    block->wrap = hybbx_parse_bool(colon + 1, 0);
                } else if (str_ieq(line_key(line), "verbs") ||
                           str_ieq(line_key(line), "continuation")) {
                    if (str_ieq(line_key(line), "continuation")) {
                        block->continuation = 1;
                    }
                    parse_bracket_list(colon + 1, block->verbs,
                                       HYBBX_COMMANDS_VERBS_PER_GROUP,
                                       &block->verb_count);
                } else if (str_ieq(line_key(line), "omit")) {
                    parse_bracket_list(colon + 1, block->omit,
                                       HYBBX_COMMANDS_VERBS_PER_GROUP,
                                       &block->omit_count);
                } else if (str_ieq(line_key(line), "optional")) {
                    parse_bracket_list(colon + 1, block->optional, 4,
                                       &block->optional_count);
                } else if (str_ieq(line_key(line), "continuation_group")) {
                    hybbx_strlcpy(block->group, colon + 1, sizeof(block->group));
                    block->continuation = 1;
                }
            }
            continue;
        }

        if (section == SEC_MENU) {
            if (indent == 2 && line[strlen(line) - 1] == ':') {
                line[strlen(line) - 1] = '\0';
                if (reg->menu_level_count < HYBBX_COMMANDS_MENU_LEVELS) {
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
                for (i = 0; i < reg->menu_level_count; i++) {
                    if (!str_ieq(reg->menu_levels[i], cur_menu_level)) {
                        continue;
                    }
                    unsigned n = reg->menu_level_layout_count[i];

                    if (n >= HYBBX_COMMANDS_MENU_BLOCKS) {
                        break;
                    }
                    line++;
                    trim_inplace(line);
                    hybbx_strlcpy(reg->menu_level_layouts[i][n], line, 32);
                    reg->menu_level_layout_count[i]++;
                    break;
                }
            }
            continue;
        }

        if (section == SEC_INDEX) {
            if (indent >= 2 && line[0] == '-') {
                if (reg->index_layout_count < HYBBX_COMMANDS_MENU_BLOCKS) {
                    line++;
                    trim_inplace(line);
                    hybbx_strlcpy(reg->index_layouts[reg->index_layout_count],
                                  line, 32);
                    reg->index_layout_count++;
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
                    hybbx_strlcpy(cmd->verb, line_key(line), sizeof(cmd->verb));
                    hybbx_strlcpy(cur_command, line_key(line),
                                  sizeof(cur_command));
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

    if (reg->menu_header[0] == '\0' || reg->command_count == 0) {
        return HYBBX_ERR_INVALID;
    }

    build_alias_lines(reg);
    reg->loaded = 1;
    return HYBBX_OK;
}

hybbx_result_t hybbx_commands_registry_init(void)
{
    char path[HYBBX_PATH_MAX];
    hybbx_result_t rc;

    hybbx_commands_registry_shutdown();

    if (hybbx_path_resolve(path, sizeof(path), HYBBX_FILE_COMMANDS) == HYBBX_OK) {
        rc = parse_commands_yaml(&g_registry, path);
        if (rc == HYBBX_OK) {
            printf("[commands] loaded %s (%u verbs)\n", path,
                   g_registry.command_count);
            return HYBBX_OK;
        }
    }

    if (hybbx_path_resolve(path, sizeof(path), "share/commands.yaml") ==
        HYBBX_OK) {
        rc = parse_commands_yaml(&g_registry, path);
        if (rc == HYBBX_OK) {
            printf("[commands] loaded %s (%u verbs)\n", path,
                   g_registry.command_count);
            return HYBBX_OK;
        }
    }

    fprintf(stderr, "[commands] failed to load " HYBBX_FILE_COMMANDS "\n");
    return HYBBX_ERR_IO;
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

static int cmd_help_shows_deleteme(hybbx_user_level_t level)
{
    return !hybbx_user_level_is_sysop(level) &&
           !hybbx_user_level_is_guest(level);
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

    if (def->only_level != 0 && level != (hybbx_user_level_t)def->only_level) {
        return 0;
    }

    if (level > def->min_level) {
        return 0;
    }

    if (str_ieq(canonical, "register")) {
        return hybbx_auth_may_register(level);
    }
    if (str_ieq(canonical, "changeme")) {
        return hybbx_auth_may_changeme(level);
    }
    if (str_ieq(canonical, "usercreate")) {
        return hybbx_auth_may_create_user(level);
    }
    if (str_ieq(canonical, "activate")) {
        return hybbx_auth_may_activate(level);
    }
    if (str_ieq(canonical, "changeuser")) {
        return hybbx_user_level_is_sysop_or_admin(level);
    }
    if (str_ieq(canonical, "deleteuser")) {
        return hybbx_user_level_is_sysop(level);
    }
    if (str_ieq(canonical, "promote") || str_ieq(canonical, "demote") ||
        str_ieq(canonical, "delete")) {
        return level == HYBBX_LEVEL_SYSOP || level == HYBBX_LEVEL_ADMIN;
    }
    if (str_ieq(canonical, "shutdown") || str_ieq(canonical, "restart") ||
        str_ieq(canonical, "broadcast") || str_ieq(canonical, "announce")) {
        return hybbx_user_level_is_sysop(level);
    }

    return 1;
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

static void render_menu_block(hybbx_session_t *session,
                              const menu_block_t *block, int show_optional)
{
    char cmds[HYBBX_LINE_MAX];
    char line1[HYBBX_LINE_MAX];
    char verbs[HYBBX_COMMANDS_VERBS_PER_GROUP][HYBBX_CMD_VERB_MAX];
    unsigned count = 0;
    unsigned i;
    const cmd_group_t *grp;

    if (block->continuation) {
        format_verbs_line(cmds, sizeof(cmds), block->verbs, block->verb_count);
        emit_menu_line(session, "", cmds, 1);
        return;
    }

    if (block->verb_count > 0) {
        count = block->verb_count;
        memcpy(verbs, block->verbs, sizeof(verbs));
    } else if (block->group[0] != '\0') {
        grp = group_find(&g_registry, block->group);
        if (grp == NULL) {
            return;
        }
        for (i = 0; i < grp->count && count < HYBBX_COMMANDS_VERBS_PER_GROUP;
             i++) {
            if (verb_in_list(grp->verbs[i], block->omit, block->omit_count)) {
                continue;
            }
            hybbx_strlcpy(verbs[count], grp->verbs[i], HYBBX_CMD_VERB_MAX);
            count++;
        }
    }

    if (show_optional) {
        for (i = 0; i < block->optional_count &&
                    count < HYBBX_COMMANDS_VERBS_PER_GROUP;
             i++) {
            hybbx_strlcpy(verbs[count], block->optional[i], HYBBX_CMD_VERB_MAX);
            count++;
        }
    }

    format_verbs_line(cmds, sizeof(cmds), verbs, count);

    if (!block->wrap) {
        emit_menu_line(session, block->label, cmds, 0);
        return;
    }

    snprintf(line1, sizeof(line1), "  %-10s %s", block->label, "");
    hybbx_session_write_line(session, line1);
    emit_menu_line(session, "", cmds, 1);
}

static void render_layout(hybbx_session_t *session, const char *layout_name,
                          int show_optional)
{
    menu_layout_t *layout = layout_find(&g_registry, layout_name);
    unsigned i;

    if (layout == NULL) {
        return;
    }

    for (i = 0; i < layout->block_count; i++) {
        render_menu_block(session, &layout->blocks[i], show_optional);
    }
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

static void render_menu_for_level(hybbx_session_t *session,
                                  hybbx_user_level_t level, int index_mode)
{
    const char *level_name = menu_level_name(level);
    unsigned i;
    unsigned j;

    for (i = 0; i < g_registry.menu_level_count; i++) {
        if (!str_ieq(g_registry.menu_levels[i], level_name)) {
            continue;
        }

        for (j = 0; j < g_registry.menu_level_layout_count[i]; j++) {
            const char *layout_name = g_registry.menu_level_layouts[i][j];
            int show_optional = cmd_help_shows_deleteme(level);

            if (!index_mode &&
                (str_ieq(layout_name, "admin") ||
                 str_ieq(layout_name, "admin_full"))) {
                if (hybbx_auth_may_create_user(level)) {
                    render_layout(session, "admin_full", show_optional);
                } else if (hybbx_auth_may_activate(level)) {
                    render_layout(session, "admin_activate", show_optional);
                }
                continue;
            }

            render_layout(session, layout_name, show_optional);
        }
        return;
    }
}

void hybbx_commands_registry_show_menu(hybbx_session_t *session)
{
    hybbx_user_level_t level;

    if (session == NULL || !g_registry.loaded) {
        return;
    }

    level = hybbx_session_user_level(session);
    hybbx_session_write_line(session, g_registry.menu_header);
    render_menu_for_level(session, level, 0);
}

void hybbx_commands_registry_show_index(hybbx_session_t *session)
{
    unsigned i;

    if (session == NULL || !g_registry.loaded) {
        return;
    }

    hybbx_session_write_line(session, g_registry.index_header);
    for (i = 0; i < g_registry.index_layout_count; i++) {
        render_layout(session, g_registry.index_layouts[i], 1);
    }
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
