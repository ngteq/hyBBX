#include "hybbx/mail.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define HYBBX_MAIL_DIR_NAME "mail"
#define HYBBX_MAIL_INBOX_NAME "inbox"
#define HYBBX_MAIL_RECYCLE_NAME "recycle"
#define HYBBX_MAIL_NEXT_FILE "mail.next"

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

static hybbx_result_t read_counter(const char *path, uint64_t *value)
{
    FILE *fp;
    unsigned long long n = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        *value = 0;
        return HYBBX_OK;
    }

    if (fscanf(fp, "%llu", &n) != 1) {
        fclose(fp);
        return HYBBX_ERR_IO;
    }

    fclose(fp);
    *value = (uint64_t)n;
    return HYBBX_OK;
}

static hybbx_result_t write_counter(const char *path, uint64_t value)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "%llu\n", (unsigned long long)value);
    fclose(fp);
    return HYBBX_OK;
}

static hybbx_result_t mail_user_inbox_path(const hybbx_mail_config_t *mail,
                                           const char *username,
                                           char *out, size_t out_len)
{
    char user_dir[HYBBX_PATH_MAX];
    char user_norm[HYBBX_USER_NAME_MAX];

    if (mail == NULL || username == NULL || username[0] == '\0' ||
        out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(user_norm, username, sizeof(user_norm));
    hybbx_username_normalize(user_norm);

    if (hybbx_path_join(user_dir, sizeof(user_dir), mail->root, user_norm) !=
        HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, user_dir, HYBBX_MAIL_INBOX_NAME);
}

static hybbx_result_t mail_user_recycle_path(const hybbx_mail_config_t *mail,
                                             const char *username,
                                             char *out, size_t out_len)
{
    char user_dir[HYBBX_PATH_MAX];
    char user_norm[HYBBX_USER_NAME_MAX];

    if (mail == NULL || username == NULL || username[0] == '\0' ||
        out == NULL || out_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(user_norm, username, sizeof(user_norm));
    hybbx_username_normalize(user_norm);

    if (hybbx_path_join(user_dir, sizeof(user_dir), mail->root, user_norm) !=
        HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, user_dir, HYBBX_MAIL_RECYCLE_NAME);
}

static int parse_msg_filename(const char *name, uint64_t *out_id)
{
    char *end;
    unsigned long long id;

    if (name == NULL || out_id == NULL) {
        return 0;
    }

    if (strlen(name) < 5 || strcmp(name + strlen(name) - 4, ".msg") != 0) {
        return 0;
    }

    id = strtoull(name, &end, 10);
    if (end == name || strcmp(end, ".msg") != 0) {
        return 0;
    }

    *out_id = (uint64_t)id;
    return 1;
}

static int mail_entry_compare(const void *a, const void *b)
{
    const hybbx_mail_entry_t *ea = (const hybbx_mail_entry_t *)a;
    const hybbx_mail_entry_t *eb = (const hybbx_mail_entry_t *)b;

    if (ea->received_at > eb->received_at) {
        return -1;
    }
    if (ea->received_at < eb->received_at) {
        return 1;
    }
    if (ea->id > eb->id) {
        return -1;
    }
    if (ea->id < eb->id) {
        return 1;
    }
    return 0;
}

static hybbx_result_t load_inbox(const char *inbox_path,
                                 hybbx_mail_entry_t *entries,
                                 size_t max_entries,
                                 size_t *out_count)
{
    DIR *dir;
    struct dirent *ent;
    size_t count = 0;

    if (inbox_path == NULL || entries == NULL || out_count == NULL) {
        return HYBBX_ERR_INVALID;
    }

    *out_count = 0;

    dir = opendir(inbox_path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return HYBBX_OK;
        }
        return HYBBX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        uint64_t id;
        char path[HYBBX_PATH_MAX];
        FILE *fp;
        char line[HYBBX_LINE_MAX];
        hybbx_mail_entry_t entry;

        if (!parse_msg_filename(ent->d_name, &id)) {
            continue;
        }

        if (count >= max_entries) {
            break;
        }

        memset(&entry, 0, sizeof(entry));
        entry.id = id;

        if (hybbx_path_join(path, sizeof(path), inbox_path, ent->d_name) !=
            HYBBX_OK) {
            continue;
        }

        fp = fopen(path, "r");
        if (fp == NULL) {
            continue;
        }

        while (fgets(line, sizeof(line), fp) != NULL) {
            char *eq;
            char *key;
            char *value;

            if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
                break;
            }

            eq = strchr(line, '=');
            if (eq == NULL) {
                continue;
            }

            *eq = '\0';
            key = line;
            value = eq + 1;

            while (*value == ' ' || *value == '\t') {
                value++;
            }

            {
                size_t vlen = strlen(value);

                while (vlen > 0 && (value[vlen - 1] == '\n' ||
                                    value[vlen - 1] == '\r')) {
                    value[--vlen] = '\0';
                }
            }

            if (str_ieq(key, "from")) {
                hybbx_strlcpy(entry.from, value, sizeof(entry.from));
            } else if (str_ieq(key, "subject")) {
                hybbx_strlcpy(entry.subject, value, sizeof(entry.subject));
            } else if (str_ieq(key, "time")) {
                entry.received_at = (time_t)strtol(value, NULL, 10);
            } else if (str_ieq(key, "read")) {
                entry.read = hybbx_bool_is_true(value);
            }
        }

        fclose(fp);
        entries[count++] = entry;
    }

    closedir(dir);

    if (count > 1) {
        qsort(entries, count, sizeof(entries[0]), mail_entry_compare);
    }

    *out_count = count;
    return HYBBX_OK;
}

static hybbx_result_t msg_file_path(const char *inbox_path, uint64_t id,
                                    char *out, size_t out_len)
{
    char name[32];

    snprintf(name, sizeof(name), "%06llu.msg",
             (unsigned long long)id);
    return hybbx_path_join(out, out_len, inbox_path, name);
}

void hybbx_mail_config_defaults(hybbx_mail_config_t *mail)
{
    if (mail == NULL) {
        return;
    }

    mail->enabled = 1;
    mail->max_messages = HYBBX_MAIL_MAX_MESSAGES;
    mail->subject_max = HYBBX_MAIL_SUBJECT_MAX;
    mail->body_max = HYBBX_MAIL_BODY_MAX;
    mail->recycle_days = HYBBX_MAIL_DEFAULT_RECYCLE_DAYS;
    mail->root[0] = '\0';
}

void hybbx_mail_config_apply(hybbx_mail_config_t *mail,
                             const hybbx_config_t *config,
                             const char *storage_path)
{
    if (mail == NULL) {
        return;
    }

    hybbx_mail_config_defaults(mail);

    if (config != NULL) {
        mail->enabled = hybbx_config_get_bool(config, "mail", "enabled", 1);
        mail->max_messages = hybbx_config_get_uint(
            config, "mail", "max_messages", HYBBX_MAIL_MAX_MESSAGES, 1u,
            HYBBX_MAIL_MAX_MESSAGES);
        mail->subject_max = hybbx_config_get_uint(
            config, "mail", "subject_max", HYBBX_MAIL_SUBJECT_MAX, 8u,
            HYBBX_MAIL_SUBJECT_MAX);
        mail->body_max = hybbx_config_get_uint(
            config, "mail", "body_max", HYBBX_MAIL_BODY_MAX, 64u,
            HYBBX_MAIL_BODY_MAX);
        mail->recycle_days = hybbx_config_get_uint(
            config, "mail", "recycle_days", HYBBX_MAIL_DEFAULT_RECYCLE_DAYS,
            1u, 365u);
    }

    mail->root[0] = '\0';
    if (storage_path != NULL && storage_path[0] != '\0') {
        const char *subdir = "mail";
        const char *custom;

        if (config != NULL) {
            custom = hybbx_config_get(config, "mail", "path", NULL);
            if (custom != NULL && custom[0] != '\0') {
                subdir = custom;
            }
        }

        if (hybbx_path_join(mail->root, sizeof(mail->root), storage_path,
                            subdir) != HYBBX_OK) {
            mail->root[0] = '\0';
        }
    }

    printf("[mail] enabled=%s max_messages=%u recycle_days=%u root=%s\n",
           hybbx_bool_to_string(mail->enabled), mail->max_messages,
           mail->recycle_days,
           mail->root[0] != '\0' ? mail->root : "(unset)");
}

static hybbx_result_t mail_ensure_root(const hybbx_mail_config_t *mail)
{
    char next_path[HYBBX_PATH_MAX];

    if (mail == NULL || !mail->enabled || mail->root[0] == '\0') {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (mkdir_p(mail->root) != 0) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_join(next_path, sizeof(next_path), mail->root,
                        HYBBX_MAIL_NEXT_FILE) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    {
        FILE *fp = fopen(next_path, "r");

        if (fp == NULL) {
            return write_counter(next_path, 0);
        }
        fclose(fp);
    }

    return HYBBX_OK;
}

static hybbx_result_t mail_next_id(const hybbx_mail_config_t *mail,
                                   uint64_t *out_id)
{
    char next_path[HYBBX_PATH_MAX];
    uint64_t id;
    hybbx_result_t rc;

    if (hybbx_path_join(next_path, sizeof(next_path), mail->root,
                        HYBBX_MAIL_NEXT_FILE) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    rc = read_counter(next_path, &id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    id++;
    rc = write_counter(next_path, id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    *out_id = id;
    return HYBBX_OK;
}

static hybbx_result_t mail_trim_inbox(const char *inbox_path,
                                      unsigned max_messages)
{
    hybbx_mail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count;
    size_t i;
    hybbx_result_t rc;

    rc = load_inbox(inbox_path, entries, HYBBX_MAIL_MAX_MESSAGES, &count);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (count <= max_messages) {
        return HYBBX_OK;
    }

    for (i = max_messages; i < count; i++) {
        char path[HYBBX_PATH_MAX];

        if (msg_file_path(inbox_path, entries[i].id, path, sizeof(path)) ==
            HYBBX_OK) {
            remove(path);
        }
    }

    return HYBBX_OK;
}

void hybbx_mail_list_inbox(hybbx_service_t *service, hybbx_session_t *session)
{
    hybbx_mail_list_inbox_range(service, session, 1, 0);
}

static const char *mail_display_name(hybbx_service_t *service,
                                     const char *name,
                                     char *buf, size_t buf_len)
{
    hybbx_storage_t *storage;
    hybbx_user_record_t user;

    if (name == NULL || name[0] == '\0') {
        return "";
    }

    storage = hybbx_service_get_storage(service);
    if (storage != NULL &&
        hybbx_storage_resolve_user(storage, name, &user) == HYBBX_OK) {
        return hybbx_user_display_name(&user);
    }

    hybbx_strlcpy(buf, name, buf_len);
    return buf;
}

static void mail_list_print(hybbx_service_t *service,
                            hybbx_session_t *session,
                            const hybbx_mail_entry_t *entries,
                            size_t count,
                            unsigned from,
                            unsigned to)
{
    size_t i;
    size_t start;
    size_t end;
    char line[HYBBX_MAIL_SUBJECT_MAX + 64];
    char header[48];
    char from_name[HYBBX_USER_NAME_MAX];

    if (to == 0 || to > count) {
        to = (unsigned)count;
    }

    if (count == 0) {
        hybbx_session_write_line(session, "Inbox empty.");
        hybbx_session_write_line(session,
            "Use /mail send <user> <subject> to compose.");
        return;
    }

    if (from == 0 || from > count || from > to) {
        hybbx_session_write_line(session, "No messages in that range.");
        return;
    }

    start = (size_t)(from - 1);
    end = (size_t)to;

    if (from == 1 && to == (unsigned)count) {
        hybbx_session_write_line(session, "Inbox (newest first):");
    } else {
        snprintf(header, sizeof(header), "Inbox %u-%u (newest first):",
                 from, to);
        hybbx_session_write_line(session, header);
    }

    for (i = start; i < end; i++) {
        snprintf(line, sizeof(line), "  %zu  %s%s  %s",
                 i + 1,
                 entries[i].read ? " " : "* ",
                 mail_display_name(service, entries[i].from, from_name,
                                   sizeof(from_name)),
                 entries[i].subject[0] != '\0' ? entries[i].subject
                                               : "(no subject)");
        hybbx_session_write_line(session, line);
    }

    hybbx_session_write_line(session,
        "  /mail read <n>  delete <n|from-to>  list <from-to>  recycle");
}

static unsigned mail_recycle_purge_expired(const hybbx_mail_config_t *mail,
                                           const char *recycle_path)
{
    DIR *dir;
    struct dirent *ent;
    time_t now = time(NULL);
    time_t max_age;
    unsigned removed = 0;

    if (mail == NULL || recycle_path == NULL || recycle_path[0] == '\0') {
        return 0;
    }

    max_age = (time_t)mail->recycle_days * 86400;

    dir = opendir(recycle_path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return 0;
        }
        return 0;
    }

    while ((ent = readdir(dir)) != NULL) {
        uint64_t id;
        char path[HYBBX_PATH_MAX];
        struct stat st;

        if (!parse_msg_filename(ent->d_name, &id)) {
            continue;
        }

        if (hybbx_path_join(path, sizeof(path), recycle_path, ent->d_name) !=
            HYBBX_OK) {
            continue;
        }

        if (stat(path, &st) != 0) {
            continue;
        }

        if (now - st.st_mtime >= max_age) {
            if (remove(path) == 0) {
                removed++;
            }
        }
    }

    closedir(dir);
    return removed;
}

static hybbx_result_t mail_move_id_to_recycle(const char *inbox_path,
                                              const char *recycle_path,
                                              uint64_t id)
{
    char src[HYBBX_PATH_MAX];
    char dst[HYBBX_PATH_MAX];
    FILE *in_fp;
    FILE *out_fp;
    char line[HYBBX_LINE_MAX];
    char tmp[HYBBX_PATH_MAX + 8];
    int has_deleted = 0;

    if (msg_file_path(inbox_path, id, src, sizeof(src)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (mkdir_p(recycle_path) != 0) {
        return HYBBX_ERR_IO;
    }

    if (msg_file_path(recycle_path, id, dst, sizeof(dst)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (strlen(dst) + 4 >= sizeof(tmp)) {
        return HYBBX_ERR_IO;
    }
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);

    in_fp = fopen(src, "r");
    out_fp = fopen(tmp, "w");
    if (in_fp == NULL || out_fp == NULL) {
        if (in_fp != NULL) {
            fclose(in_fp);
        }
        if (out_fp != NULL) {
            fclose(out_fp);
        }
        remove(tmp);
        return HYBBX_ERR_IO;
    }

    while (fgets(line, sizeof(line), in_fp) != NULL) {
        if (strncmp(line, "deleted=", 8) == 0) {
            continue;
        }
        if (!has_deleted && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            fprintf(out_fp, "deleted=%ld\n", (long)time(NULL));
            has_deleted = 1;
        }
        fputs(line, out_fp);
    }

    if (!has_deleted) {
        fprintf(out_fp, "deleted=%ld\n", (long)time(NULL));
    }

    fclose(in_fp);
    fclose(out_fp);

    if (remove(src) != 0) {
        remove(tmp);
        return HYBBX_ERR_IO;
    }

    if (rename(tmp, dst) != 0) {
        remove(tmp);
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static hybbx_result_t mail_purge_user_recycle(const hybbx_mail_config_t *mail,
                                              const char *username)
{
    char recycle[HYBBX_PATH_MAX];

    if (mail_user_recycle_path(mail, username, recycle, sizeof(recycle)) !=
        HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    (void)mail_recycle_purge_expired(mail, recycle);
    return HYBBX_OK;
}

void hybbx_mail_list_inbox_range(hybbx_service_t *service,
                                 hybbx_session_t *session,
                                 unsigned from,
                                 unsigned to)
{
    const hybbx_mail_config_t *mail;
    hybbx_mail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    char inbox[HYBBX_PATH_MAX];
    size_t count;
    hybbx_result_t rc;

    if (service == NULL || session == NULL) {
        return;
    }

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use mail.");
        return;
    }

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        hybbx_session_write_line(session, "Mail is disabled.");
        return;
    }

    rc = mail_ensure_root(mail);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Mail storage unavailable.");
        return;
    }

    rc = mail_user_inbox_path(mail, hybbx_session_username(session),
                              inbox, sizeof(inbox));
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Mail path error.");
        return;
    }

    (void)mail_purge_user_recycle(mail, hybbx_session_username(session));

    rc = load_inbox(inbox, entries, HYBBX_MAIL_MAX_MESSAGES, &count);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Cannot read inbox.");
        return;
    }

    mail_list_print(service, session, entries, count, from, to);
}

int hybbx_mail_parse_list_range(const char *spec,
                                unsigned *from, unsigned *to)
{
    const char *dash;
    char *end;
    unsigned long start;
    unsigned long end_num;

    if (from == NULL || to == NULL) {
        return 0;
    }

    if (spec == NULL || spec[0] == '\0') {
        *from = 1;
        *to = 0;
        return 1;
    }

    dash = strchr(spec, '-');
    if (dash == NULL) {
        start = strtoul(spec, &end, 10);
        if (end == spec || *end != '\0' || start == 0) {
            return 0;
        }
        *from = (unsigned)start;
        *to = (unsigned)start;
        return 1;
    }

    start = strtoul(spec, &end, 10);
    if (end != dash || start == 0) {
        return 0;
    }

    end_num = strtoul(dash + 1, &end, 10);
    if (end == dash + 1 || *end != '\0' || end_num == 0) {
        return 0;
    }

    if (start > end_num) {
        return 0;
    }

    *from = (unsigned)start;
    *to = (unsigned)end_num;
    return 1;
}

static hybbx_result_t mail_resolve_index(hybbx_service_t *service,
                                         hybbx_session_t *session,
                                         unsigned list_index,
                                         uint64_t *out_id,
                                         char *inbox_path, size_t inbox_len)
{
    const hybbx_mail_config_t *mail;
    hybbx_mail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count;
    hybbx_result_t rc;

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    rc = mail_ensure_root(mail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = mail_user_inbox_path(mail, hybbx_session_username(session),
                              inbox_path, inbox_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = load_inbox(inbox_path, entries, HYBBX_MAIL_MAX_MESSAGES, &count);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (list_index == 0 || list_index > count) {
        return HYBBX_ERR_NOT_FOUND;
    }

    *out_id = entries[list_index - 1].id;
    return HYBBX_OK;
}

static void mail_print_header_line(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const char *line)
{
    char out_line[HYBBX_LINE_MAX];
    char name_buf[HYBBX_USER_NAME_MAX];
    char fallback[HYBBX_USER_NAME_MAX];
    const char *value;
    const char *display;
    size_t vlen;

    if (line == NULL || session == NULL) {
        return;
    }

    if (strncmp(line, "from=", 5) == 0) {
        value = line + 5;
        vlen = strlen(value);
        while (vlen > 0 && (value[vlen - 1] == '\n' || value[vlen - 1] == '\r')) {
            vlen--;
        }
        memcpy(name_buf, value, vlen);
        name_buf[vlen] = '\0';
        display = mail_display_name(service, name_buf, fallback, sizeof(fallback));
        snprintf(out_line, sizeof(out_line), "from=%s", display);
        hybbx_session_write_line(session, out_line);
        return;
    }

    if (strncmp(line, "to=", 3) == 0) {
        value = line + 3;
        vlen = strlen(value);
        while (vlen > 0 && (value[vlen - 1] == '\n' || value[vlen - 1] == '\r')) {
            vlen--;
        }
        memcpy(name_buf, value, vlen);
        name_buf[vlen] = '\0';
        display = mail_display_name(service, name_buf, fallback, sizeof(fallback));
        snprintf(out_line, sizeof(out_line), "to=%s", display);
        hybbx_session_write_line(session, out_line);
        return;
    }

    hybbx_session_write_line(session, line);
}

hybbx_result_t hybbx_mail_read(hybbx_service_t *service,
                               hybbx_session_t *session,
                               unsigned list_index)
{
    char inbox[HYBBX_PATH_MAX];
    char path[HYBBX_PATH_MAX];
    uint64_t id;
    FILE *fp;
    char line[HYBBX_LINE_MAX];
    int in_body = 0;
    hybbx_result_t rc;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use mail.");
        return HYBBX_ERR_DENIED;
    }

    (void)mail_purge_user_recycle(hybbx_service_get_mail(service),
                                  hybbx_session_username(session));

    rc = mail_resolve_index(service, session, list_index, &id,
                            inbox, sizeof(inbox));
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "No such message.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (msg_file_path(inbox, id, path, sizeof(path)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        hybbx_session_write_line(session, "Cannot open message.");
        return HYBBX_ERR_IO;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!in_body) {
            if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
                in_body = 1;
                continue;
            }
            if (strncmp(line, "read=", 5) == 0) {
                continue;
            }
            if (strncmp(line, "from=", 5) == 0 ||
                strncmp(line, "to=", 3) == 0) {
                mail_print_header_line(service, session, line);
                continue;
            }
        }
        hybbx_session_write_line(session, line);
    }

    fclose(fp);

    {
        char tmp_path[HYBBX_PATH_MAX + 8];
        FILE *in_fp;
        FILE *out_fp;
        char buf[HYBBX_LINE_MAX];

        if (strlen(path) + 4 >= sizeof(tmp_path)) {
            return HYBBX_OK;
        }
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
        in_fp = fopen(path, "r");
        out_fp = fopen(tmp_path, "w");
        if (in_fp != NULL && out_fp != NULL) {
            while (fgets(buf, sizeof(buf), in_fp) != NULL) {
                if (strncmp(buf, "read=", 5) == 0) {
                    fputs("read=yes\n", out_fp);
                } else {
                    fputs(buf, out_fp);
                }
            }
            fclose(in_fp);
            fclose(out_fp);
            remove(path);
            rename(tmp_path, path);
        } else {
            if (in_fp != NULL) {
                fclose(in_fp);
            }
            if (out_fp != NULL) {
                fclose(out_fp);
            }
            remove(tmp_path);
        }
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_mail_delete_range(hybbx_service_t *service,
                                       hybbx_session_t *session,
                                       unsigned from,
                                       unsigned to)
{
    const hybbx_mail_config_t *mail;
    hybbx_mail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    char inbox[HYBBX_PATH_MAX];
    char recycle[HYBBX_PATH_MAX];
    size_t count;
    size_t i;
    unsigned moved = 0;
    hybbx_result_t rc;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use mail.");
        return HYBBX_ERR_DENIED;
    }

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        hybbx_session_write_line(session, "Mail is disabled.");
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (to == 0) {
        to = from;
    }
    if (from == 0 || from > to) {
        hybbx_session_write_line(session, "Usage: /mail delete <n|from-to>");
        return HYBBX_OK;
    }

    rc = mail_ensure_root(mail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = mail_user_inbox_path(mail, hybbx_session_username(session),
                              inbox, sizeof(inbox));
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = mail_user_recycle_path(mail, hybbx_session_username(session),
                                recycle, sizeof(recycle));
    if (rc != HYBBX_OK) {
        return rc;
    }

    (void)mail_purge_user_recycle(mail, hybbx_session_username(session));

    rc = load_inbox(inbox, entries, HYBBX_MAIL_MAX_MESSAGES, &count);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (from > count || to > count) {
        hybbx_session_write_line(session, "No such message.");
        return HYBBX_OK;
    }

    for (i = (size_t)(from - 1); i < (size_t)to; i++) {
        rc = mail_move_id_to_recycle(inbox, recycle, entries[i].id);
        if (rc != HYBBX_OK) {
            hybbx_session_write_line(session, "Delete failed.");
            return rc;
        }
        moved++;
    }

    if (moved == 1) {
        hybbx_session_write_line(session,
            "Message moved to recycle (auto-purge after configured days).");
    } else {
        char buf[64];

        snprintf(buf, sizeof(buf),
                 "%u messages moved to recycle (auto-purge after %u days).",
                 moved, mail->recycle_days);
        hybbx_session_write_line(session, buf);
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_mail_delete(hybbx_service_t *service,
                               hybbx_session_t *session,
                               unsigned list_index)
{
    return hybbx_mail_delete_range(service, session, list_index, list_index);
}

hybbx_result_t hybbx_mail_recycle_empty(hybbx_service_t *service,
                                        hybbx_session_t *session)
{
    const hybbx_mail_config_t *mail;
    char recycle[HYBBX_PATH_MAX];
    DIR *dir;
    struct dirent *ent;
    unsigned removed = 0;
    hybbx_result_t rc;
    char buf[64];

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use mail.");
        return HYBBX_ERR_DENIED;
    }

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        hybbx_session_write_line(session, "Mail is disabled.");
        return HYBBX_ERR_UNSUPPORTED;
    }

    rc = mail_ensure_root(mail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = mail_user_recycle_path(mail, hybbx_session_username(session),
                                recycle, sizeof(recycle));
    if (rc != HYBBX_OK) {
        return rc;
    }

    (void)mail_recycle_purge_expired(mail, recycle);

    dir = opendir(recycle);
    if (dir == NULL) {
        if (errno == ENOENT) {
            hybbx_session_write_line(session, "Recycle bin empty.");
            return HYBBX_OK;
        }
        hybbx_session_write_line(session, "Cannot read recycle bin.");
        return HYBBX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        uint64_t id;
        char path[HYBBX_PATH_MAX];

        if (!parse_msg_filename(ent->d_name, &id)) {
            continue;
        }

        if (hybbx_path_join(path, sizeof(path), recycle, ent->d_name) !=
            HYBBX_OK) {
            continue;
        }

        if (remove(path) == 0) {
            removed++;
        }
    }

    closedir(dir);

    if (removed == 0) {
        hybbx_session_write_line(session, "Recycle bin empty.");
    } else {
        snprintf(buf, sizeof(buf), "Recycle bin emptied (%u message(s)).",
                 removed);
        hybbx_session_write_line(session, buf);
    }

    return HYBBX_OK;
}

#define HYBBX_MAIL_SYSTEM_FROM "system"

static void mail_format_utc_time(time_t when, char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }

    buf[0] = '\0';
    if (when <= 0) {
        return;
    }

    {
        const struct tm *tm = gmtime(&when);

        if (tm != NULL) {
            (void)strftime(buf, len, "%Y-%m-%d %H:%M:%S UTC", tm);
        }
    }
}

typedef struct mail_notify_staff_ctx {
    hybbx_service_t *service;
    char subject[HYBBX_MAIL_SUBJECT_MAX];
    char body[HYBBX_MAIL_BODY_MAX];
} mail_notify_staff_ctx_t;

static hybbx_result_t mail_notify_staff_cb(const hybbx_user_record_t *user,
                                           void *ctx)
{
    mail_notify_staff_ctx_t *nctx = (mail_notify_staff_ctx_t *)ctx;

    if (user == NULL || nctx == NULL) {
        return HYBBX_OK;
    }

    if (!user->active || !hybbx_user_level_is_sysop_or_admin(user->level)) {
        return HYBBX_OK;
    }

    (void)hybbx_mail_deliver(nctx->service, HYBBX_MAIL_SYSTEM_FROM,
                             user->username, nctx->subject, nctx->body);
    return HYBBX_OK;
}

hybbx_result_t hybbx_mail_notify_staff_registration(hybbx_service_t *service,
                                                    const hybbx_user_registration_t *reg,
                                                    const hybbx_user_record_t *user)
{
    const hybbx_mail_config_t *mail;
    hybbx_storage_t *storage;
    mail_notify_staff_ctx_t ctx;
    char created[48];
    int n;

    if (service == NULL || reg == NULL || user == NULL) {
        return HYBBX_ERR_INVALID;
    }

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    n = snprintf(ctx.subject, sizeof(ctx.subject),
                 "Registration pending: %s", reg->nickname);
    if (n < 0 || (size_t)n >= sizeof(ctx.subject)) {
        return HYBBX_ERR_INVALID;
    }

    mail_format_utc_time(user->created_at, created, sizeof(created));
    if (created[0] == '\0') {
        hybbx_strlcpy(created, "(unknown)", sizeof(created));
    }

    n = snprintf(ctx.body, sizeof(ctx.body),
                 "Guest self-registration pending approval.\n"
                 "/register data submitted:\n\n"
                 "  Nickname:   %s\n"
                 "  Login:      %s\n"
                 "  Full name:  %s\n"
                 "  Country:    %s\n"
                 "  Location:   %s\n"
                 "  Email:      %s\n\n"
                 "Account record:\n\n"
                 "  User ID:    %llu\n"
                 "  Level:      %s\n"
                 "  Active:     %s\n"
                 "  Created:    %s\n\n"
                 "Review all fields. If correct, activate with:\n"
                 "  /activate %s\n",
                 reg->nickname,
                 reg->username,
                 reg->full_name,
                 reg->country,
                 reg->location,
                 reg->email,
                 (unsigned long long)user->id,
                 hybbx_user_level_name(user->level),
                 hybbx_bool_to_string(user->active),
                 created,
                 reg->username);
    if (n < 0 || (size_t)n >= sizeof(ctx.body)) {
        return HYBBX_ERR_INVALID;
    }

    ctx.service = service;

    (void)hybbx_storage_foreach_user(storage, mail_notify_staff_cb, &ctx);
    return HYBBX_OK;
}

hybbx_result_t hybbx_mail_deliver(hybbx_service_t *service,
                                  const char *from_user,
                                  const char *to_user,
                                  const char *subject,
                                  const char *body)
{
    const hybbx_mail_config_t *mail;
    hybbx_storage_t *storage;
    hybbx_user_record_t recipient;
    char inbox[HYBBX_PATH_MAX];
    char path[HYBBX_PATH_MAX];
    uint64_t id;
    FILE *fp;
    hybbx_result_t rc;
    char to_norm[HYBBX_USER_NAME_MAX];
    size_t body_len;

    if (service == NULL || from_user == NULL || to_user == NULL) {
        return HYBBX_ERR_INVALID;
    }

    mail = hybbx_service_get_mail(service);
    if (mail == NULL || !mail->enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (subject == NULL) {
        subject = "";
    }
    if (body == NULL) {
        body = "";
    }

    body_len = strlen(body);
    if (strlen(subject) > mail->subject_max || body_len > mail->body_max) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(to_norm, to_user, sizeof(to_norm));
    hybbx_username_normalize(to_norm);

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_resolve_user(storage, to_user, &recipient);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        return HYBBX_ERR_NOT_FOUND;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    hybbx_strlcpy(to_norm, recipient.username, sizeof(to_norm));

    if (hybbx_user_level_is_guest(recipient.level) || !recipient.active) {
        return HYBBX_ERR_DENIED;
    }

    rc = mail_ensure_root(mail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = mail_user_inbox_path(mail, to_norm, inbox, sizeof(inbox));
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (mkdir_p(inbox) != 0) {
        return HYBBX_ERR_IO;
    }

    rc = mail_next_id(mail, &id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (msg_file_path(inbox, id, path, sizeof(path)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "id=%llu\n", (unsigned long long)id);
    fprintf(fp, "from=%s\n", from_user);
    fprintf(fp, "to=%s\n", to_norm);
    fprintf(fp, "subject=%s\n", subject);
    fprintf(fp, "time=%ld\n", (long)time(NULL));
    fprintf(fp, "read=no\n");
    fprintf(fp, "---\n");
    fputs(body, fp);
    if (body_len == 0 || body[body_len - 1] != '\n') {
        fputc('\n', fp);
    }
    fclose(fp);

    (void)mail_trim_inbox(inbox, mail->max_messages);
    return HYBBX_OK;
}
