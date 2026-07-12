/*
 * Link/repeater edge registry: data/links/<id>.ini, [link.<id>], stale prune.
 */
#include "hybbx/link.h"
#include "hybbx/config.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static char *link_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

static const char *LINK_CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static int mkdir_p(const char *path)
{
    char buf[HYBBX_PATH_MAX];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    len = strlen(path);
    if (len >= sizeof(buf)) {
        return -1;
    }

    memcpy(buf, path, len + 1);
    for (i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            buf[i] = '/';
        }
    }

    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

void hybbx_link_generate_code(char *out, size_t out_cap)
{
    static unsigned seed;
    size_t i;
    size_t n;

    if (out == NULL || out_cap < 2) {
        return;
    }

    if (seed == 0) {
        seed = (unsigned)time(NULL) ^ 0x48594242u;
    }

    n = out_cap - 1;
    if (n > HYBBX_LINK_CODE_MAX - 1) {
        n = HYBBX_LINK_CODE_MAX - 1;
    }

    for (i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        out[i] = LINK_CODE_CHARS[(seed >> 16) % 32];
    }
    out[n] = '\0';
}

void hybbx_link_registry_init(hybbx_link_registry_t *reg,
                              const char *data_dir,
                              const char *config_path,
                              unsigned stale_days)
{
    if (reg == NULL) {
        return;
    }

    memset(reg, 0, sizeof(*reg));
    if (data_dir != NULL && data_dir[0] != '\0') {
        snprintf(reg->dir, sizeof(reg->dir), "%s/links", data_dir);
    }
    if (config_path != NULL) {
        hybbx_strlcpy(reg->config_path, config_path, sizeof(reg->config_path));
    }
    reg->stale_days = stale_days > 0 ? stale_days : HYBBX_LINK_STALE_DAYS;
}

static hybbx_result_t link_shard_path(const hybbx_link_registry_t *reg,
                                      const char *id,
                                      char *out, size_t out_cap)
{
    if (reg == NULL || id == NULL || id[0] == '\0' || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (snprintf(out, out_cap, "%s/%s.ini", reg->dir, id) >= (int)out_cap) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

static hybbx_result_t read_link_last_seen(const char *path, time_t *last_seen)
{
    FILE *fp;
    char line[256];
    time_t value = 0;

    if (path == NULL || last_seen == NULL) {
        return HYBBX_ERR_INVALID;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        *last_seen = 0;
        return HYBBX_OK;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "last_seen=", 10) == 0) {
            value = (time_t)strtol(line + 10, NULL, 10);
        }
    }

    fclose(fp);
    *last_seen = value;
    return HYBBX_OK;
}

static hybbx_result_t write_link_shard(const char *path,
                                       const char *id,
                                       const char *role,
                                       const char *link_code,
                                       time_t now,
                                       int is_new)
{
    FILE *fp;
    time_t created = now;
    char existing_code[HYBBX_LINK_CODE_MAX];

    existing_code[0] = '\0';
    if (!is_new) {
        FILE *old = fopen(path, "r");
        char line[256];

        if (old != NULL) {
            while (fgets(line, sizeof(line), old) != NULL) {
                if (strncmp(line, "created=", 8) == 0) {
                    created = (time_t)strtol(line + 8, NULL, 10);
                } else if (strncmp(line, "link_code=", 10) == 0) {
                    size_t n = strcspn(line + 10, "\r\n");
                    if (n >= sizeof(existing_code)) {
                        n = sizeof(existing_code) - 1;
                    }
                    memcpy(existing_code, line + 10, n);
                    existing_code[n] = '\0';
                }
            }
            fclose(old);
        }
    }

    if (link_code != NULL && link_code[0] != '\0') {
        hybbx_strlcpy(existing_code, link_code, sizeof(existing_code));
    } else if (existing_code[0] == '\0') {
        hybbx_link_generate_code(existing_code, sizeof(existing_code));
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "id=%s\n", id);
    fprintf(fp, "role=%s\n", role != NULL && role[0] != '\0' ? role : "link");
    fprintf(fp, "link_code=%s\n", existing_code);
    fprintf(fp, "created=%ld\n", (long)created);
    fprintf(fp, "last_seen=%ld\n", (long)now);
    fclose(fp);
    return HYBBX_OK;
}

static hybbx_result_t config_upsert_link_section(const char *config_path,
                                                 const char *id,
                                                 const char *role,
                                                 const char *link_code,
                                                 time_t last_seen)
{
    hybbx_config_t cfg;
    char section[HYBBX_CONFIG_SECTION_MAX];
    char value[64];
    size_t i;
    hybbx_result_t rc;
    int found_role = 0;
    int found_code = 0;
    int found_seen = 0;

    if (config_path == NULL || config_path[0] == '\0' || id == NULL) {
        return HYBBX_OK;
    }

    rc = hybbx_config_load(&cfg, config_path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(section, sizeof(section), "link.%s", id);
    snprintf(value, sizeof(value), "%ld", (long)last_seen);

    for (i = 0; i < cfg.count; i++) {
        if (cfg.entries[i].section == NULL || cfg.entries[i].key == NULL) {
            continue;
        }
        if (strcmp(cfg.entries[i].section, section) != 0) {
            continue;
        }
        if (strcmp(cfg.entries[i].key, "role") == 0) {
            free(cfg.entries[i].value);
            cfg.entries[i].value = link_strdup(role != NULL ? role : "link");
            found_role = 1;
        } else if (strcmp(cfg.entries[i].key, "link_code") == 0) {
            free(cfg.entries[i].value);
            cfg.entries[i].value = link_strdup(link_code != NULL ? link_code : "");
            found_code = 1;
        } else if (strcmp(cfg.entries[i].key, "last_seen") == 0) {
            free(cfg.entries[i].value);
            cfg.entries[i].value = link_strdup(value);
            found_seen = 1;
        }
    }

    if (!found_role) {
        rc = hybbx_config_set(&cfg, section, "role",
                              role != NULL ? role : "link");
        if (rc != HYBBX_OK) {
            hybbx_config_free(&cfg);
            return rc;
        }
    }
    if (!found_code && link_code != NULL) {
        rc = hybbx_config_set(&cfg, section, "link_code", link_code);
        if (rc != HYBBX_OK) {
            hybbx_config_free(&cfg);
            return rc;
        }
    }
    if (!found_seen) {
        rc = hybbx_config_set(&cfg, section, "last_seen", value);
        if (rc != HYBBX_OK) {
            hybbx_config_free(&cfg);
            return rc;
        }
    }

    rc = hybbx_config_save(&cfg, config_path);
    hybbx_config_free(&cfg);
    return rc;
}

hybbx_result_t hybbx_link_registry_touch(hybbx_link_registry_t *reg,
                                         const char *id,
                                         const char *role,
                                         char *link_code_out,
                                         size_t link_code_cap)
{
    char path[HYBBX_PATH_MAX];
    char code[HYBBX_LINK_CODE_MAX];
    time_t now = time(NULL);
    struct stat st;
    hybbx_result_t rc;
    int is_new;

    if (reg == NULL || id == NULL || id[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (reg->dir[0] != '\0') {
        mkdir_p(reg->dir);
    }

    rc = link_shard_path(reg, id, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    is_new = stat(path, &st) != 0;
    code[0] = '\0';
    if (!is_new) {
        FILE *fp = fopen(path, "r");
        char line[256];

        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (strncmp(line, "link_code=", 10) == 0) {
                    size_t n = strcspn(line + 10, "\r\n");
                    if (n >= sizeof(code)) {
                        n = sizeof(code) - 1;
                    }
                    memcpy(code, line + 10, n);
                    code[n] = '\0';
                }
            }
            fclose(fp);
        }
    }
    if (code[0] == '\0') {
        hybbx_link_generate_code(code, sizeof(code));
    }

    rc = write_link_shard(path, id, role, code, now, is_new);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (link_code_out != NULL && link_code_cap > 0) {
        hybbx_strlcpy(link_code_out, code, link_code_cap);
    }

    return config_upsert_link_section(reg->config_path, id, role, code, now);
}

static hybbx_result_t remove_link_files(const hybbx_link_registry_t *reg,
                                        const char *id)
{
    char path[HYBBX_PATH_MAX];
    hybbx_result_t rc;

    rc = link_shard_path(reg, id, path, sizeof(path));
    if (rc == HYBBX_OK) {
        remove(path);
    }

    if (reg->config_path[0] != '\0') {
        hybbx_config_t cfg;
        char section[HYBBX_CONFIG_SECTION_MAX];

        snprintf(section, sizeof(section), "link.%s", id);
        if (hybbx_config_load(&cfg, reg->config_path) == HYBBX_OK) {
            hybbx_config_remove_section(&cfg, section);
            hybbx_config_save(&cfg, reg->config_path);
            hybbx_config_free(&cfg);
        }
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_link_registry_prune(hybbx_link_registry_t *reg)
{
    DIR *dir;
    struct dirent *ent;
    time_t now = time(NULL);
    time_t cutoff;
    char id[HYBBX_LINK_ID_MAX];

    if (reg == NULL || reg->dir[0] == '\0') {
        return HYBBX_OK;
    }

    cutoff = now - (time_t)reg->stale_days * 24 * 60 * 60;

    dir = opendir(reg->dir);
    if (dir == NULL) {
        return HYBBX_OK;
    }

    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        size_t nlen = strlen(name);
        time_t last_seen;
        char path[HYBBX_PATH_MAX];

        if (nlen < 5 || strcmp(name + nlen - 4, ".ini") != 0) {
            continue;
        }

        nlen -= 4;
        if (nlen >= sizeof(id)) {
            continue;
        }
        memcpy(id, name, nlen);
        id[nlen] = '\0';

        if (snprintf(path, sizeof(path), "%s/%s", reg->dir, name) >= (int)sizeof(path)) {
            continue;
        }

        if (read_link_last_seen(path, &last_seen) != HYBBX_OK) {
            continue;
        }

        if (last_seen > 0 && last_seen < cutoff) {
            hybbx_log_info("[links] removing stale link '%s' (last auth %ld days ago)",
                   id, (long)((now - last_seen) / (24 * 60 * 60)));
            remove_link_files(reg, id);
        }
    }

    closedir(dir);
    return HYBBX_OK;
}
