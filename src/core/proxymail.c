#include "hybbx/proxymail.h"
#include "hybbx/mains_proxy.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/storage.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define HYBBX_PROXYMAIL_DIR_NAME "proxymail"
#define HYBBX_PROXYMAIL_INBOX_NAME "inbox"
#define HYBBX_PROXYMAIL_RECYCLE_NAME "recycle"
#define HYBBX_PROXYMAIL_NEXT_FILE "proxymail.next"

typedef struct proxymail_entry {
    uint64_t id;
    char from[HYBBX_PROXYMAIL_ADDRESS_MAX];
    char subject[HYBBX_MAIL_SUBJECT_MAX + 1];
    time_t received_at;
    int read;
} proxymail_entry_t;

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

static hybbx_result_t proxymail_root_path(hybbx_service_t *service,
                                          char *out, size_t out_len)
{
    hybbx_storage_t *storage;
    const char *root;

    storage = hybbx_service_get_storage(service);
    root = hybbx_storage_root_path(storage);
    if (root == NULL || root[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, root, HYBBX_PROXYMAIL_DIR_NAME);
}

static hybbx_result_t proxymail_user_inbox_path(hybbx_service_t *service,
                                                const char *username,
                                                char *out, size_t out_len)
{
    char root[HYBBX_PATH_MAX];
    char user_dir[HYBBX_PATH_MAX];
    char user_norm[HYBBX_USER_NAME_MAX];

    if (username == NULL || username[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (proxymail_root_path(service, root, sizeof(root)) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(user_norm, username, sizeof(user_norm));
    hybbx_username_normalize(user_norm);

    if (hybbx_path_join(user_dir, sizeof(user_dir), root, user_norm) !=
        HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, user_dir, HYBBX_PROXYMAIL_INBOX_NAME);
}

static int parse_msg_filename(const char *name, uint64_t *id)
{
    char *end;

    if (name == NULL || id == NULL) {
        return 0;
    }

    if (strlen(name) < 5 || strcmp(name + strlen(name) - 4, ".msg") != 0) {
        return 0;
    }

    *id = (uint64_t)strtoull(name, &end, 10);
    return end != name && strcmp(end, ".msg") == 0;
}

static int proxymail_entry_cmp(const void *a, const void *b)
{
    const proxymail_entry_t *ea = (const proxymail_entry_t *)a;
    const proxymail_entry_t *eb = (const proxymail_entry_t *)b;

    if (ea->id < eb->id) {
        return 1;
    }
    if (ea->id > eb->id) {
        return -1;
    }
    return 0;
}

static hybbx_result_t proxymail_load_inbox(hybbx_service_t *service,
                                            const char *username,
                                            proxymail_entry_t *entries,
                                            size_t max_entries,
                                            size_t *out_count)
{
    char inbox_path[HYBBX_PATH_MAX];
    DIR *dir;
    struct dirent *ent;
    size_t count = 0;

    if (proxymail_user_inbox_path(service, username, inbox_path,
                                  sizeof(inbox_path)) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    *out_count = 0;

    dir = opendir(inbox_path);
    if (dir == NULL) {
        return errno == ENOENT ? HYBBX_OK : HYBBX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        uint64_t id;
        char path[HYBBX_PATH_MAX];
        FILE *fp;
        char line[HYBBX_LINE_MAX];
        proxymail_entry_t entry;

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
            char *eq = strchr(line, '=');
            char *key;
            char *value;

            if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
                break;
            }

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
    qsort(entries, count, sizeof(entries[0]), proxymail_entry_cmp);
    *out_count = count;
    return HYBBX_OK;
}

static hybbx_result_t proxymail_next_id(hybbx_service_t *service,
                                        uint64_t *out_id)
{
    char root[HYBBX_PATH_MAX];
    char counter_path[HYBBX_PATH_MAX];
    FILE *fp;
    unsigned long long n = 0;

    if (proxymail_root_path(service, root, sizeof(root)) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    if (mkdir_p(root) != 0) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_join(counter_path, sizeof(counter_path), root,
                        HYBBX_PROXYMAIL_NEXT_FILE) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    fp = fopen(counter_path, "r");
    if (fp != NULL) {
        (void)fscanf(fp, "%llu", &n);
        fclose(fp);
    }

    n++;
    *out_id = (uint64_t)n;

    fp = fopen(counter_path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "%llu\n", n);
    fclose(fp);
    return HYBBX_OK;
}

static hybbx_result_t proxymail_store_message(hybbx_service_t *service,
                                              const char *username,
                                              const char *from_address,
                                              const char *subject,
                                              const char *body)
{
    char inbox_path[HYBBX_PATH_MAX];
    char msg_path[HYBBX_PATH_MAX];
    char msg_name[32];
    uint64_t id;
    FILE *fp;
    hybbx_result_t rc;

    rc = proxymail_next_id(service, &id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (proxymail_user_inbox_path(service, username, inbox_path,
                                  sizeof(inbox_path)) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    if (mkdir_p(inbox_path) != 0) {
        return HYBBX_ERR_IO;
    }

    snprintf(msg_name, sizeof(msg_name), "%06llu.msg",
             (unsigned long long)id);
    if (hybbx_path_join(msg_path, sizeof(msg_path), inbox_path, msg_name) !=
        HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    fp = fopen(msg_path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "from=%s\n", from_address);
    fprintf(fp, "subject=%s\n", subject != NULL ? subject : "");
    fprintf(fp, "time=%ld\n", (long)time(NULL));
    fprintf(fp, "read=no\n");
    fprintf(fp, "---\n%s\n", body != NULL ? body : "");
    fclose(fp);
    return HYBBX_OK;
}

int hybbx_proxymail_parse_address(const char *address,
                                  char *user, size_t user_len,
                                  char *remote_service, size_t remote_len)
{
    const char *at;
    size_t ulen;
    size_t slen;

    if (user != NULL && user_len > 0) {
        user[0] = '\0';
    }
    if (remote_service != NULL && remote_len > 0) {
        remote_service[0] = '\0';
    }

    if (address == NULL || address[0] == '\0') {
        return 0;
    }

    at = strchr(address, '@');
    if (at == NULL || at == address || at[1] == '\0') {
        return 0;
    }

    ulen = (size_t)(at - address);
    slen = strlen(at + 1);
    if (ulen == 0 || slen == 0 || ulen >= HYBBX_USER_NAME_MAX ||
        slen >= HYBBX_PROXYMAIL_SERVICE_NAME_MAX) {
        return 0;
    }

    if (user != NULL && user_len > 0) {
        memcpy(user, address, ulen);
        user[ulen] = '\0';
        hybbx_username_normalize(user);
        if (user[0] == '\0') {
            return 0;
        }
    }

    if (remote_service != NULL && remote_len > 0) {
        hybbx_strlcpy(remote_service, at + 1, remote_len);
    }

    return 1;
}

static void proxymail_list_print(hybbx_session_t *session,
                                 const proxymail_entry_t *entries,
                                 size_t count,
                                 unsigned from,
                                 unsigned to)
{
    size_t i;
    size_t start;
    size_t end;
    char line[HYBBX_MAIL_SUBJECT_MAX + 96];
    char header[48];

    if (to == 0 || to > count) {
        to = (unsigned)count;
    }

    if (count == 0) {
        hybbx_session_write_line(session, "Inbox empty.");
        hybbx_session_write_line(session,
            "Try: /proxymail send user@mainname Your subject");
        return;
    }

    if (from == 0 || from > count || from > to) {
        hybbx_session_write_line(session, "No messages in that range.");
        return;
    }

    start = (size_t)(from - 1);
    end = (size_t)to;

    if (from == 1 && to == (unsigned)count) {
        hybbx_session_write_line(session, "Proxymail inbox (newest first):");
    } else {
        snprintf(header, sizeof(header), "Proxymail %u-%u (newest first):",
                 from, to);
        hybbx_session_write_line(session, header);
    }

    for (i = start; i < end; i++) {
        snprintf(line, sizeof(line), "  %zu  %s%s  %s",
                 i + 1,
                 entries[i].read ? " " : "* ",
                 entries[i].from,
                 entries[i].subject[0] != '\0' ? entries[i].subject
                                                : "(no subject)");
        hybbx_session_write_line(session, line);
    }

    hybbx_session_write_line(session,
        "  /proxymail read <n>  delete <n|from-to>  list <from-to>");
}

void hybbx_proxymail_list_inbox(hybbx_service_t *service,
                                hybbx_session_t *session)
{
    proxymail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count = 0;

    if (!hybbx_mains_proxy_mesh_active()) {
        hybbx_session_write_line(session,
            "Proxymail — send mail to user@another-main.");
        hybbx_session_write_line(session,
            "No peer links active — check mains_proxy configuration.");
    }

    if (proxymail_load_inbox(service, hybbx_session_username(session),
                                   entries, HYBBX_MAIL_MAX_MESSAGES,
                                   &count) != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not read proxymail inbox.");
        return;
    }

    proxymail_list_print(session, entries, count, 1, (unsigned)count);
}

void hybbx_proxymail_list_inbox_range(hybbx_service_t *service,
                                      hybbx_session_t *session,
                                      unsigned from, unsigned to)
{
    proxymail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count = 0;

    if (proxymail_load_inbox(service, hybbx_session_username(session),
                                   entries, HYBBX_MAIL_MAX_MESSAGES,
                                   &count) != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not read proxymail inbox.");
        return;
    }

    proxymail_list_print(session, entries, count, from, to);
}

hybbx_result_t hybbx_proxymail_read(hybbx_service_t *service,
                                    hybbx_session_t *session,
                                    unsigned list_index)
{
    proxymail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count = 0;
    const proxymail_entry_t *entry;
    char inbox_path[HYBBX_PATH_MAX];
    char msg_path[HYBBX_PATH_MAX];
    char msg_name[32];
    FILE *fp;
    char line[HYBBX_LINE_MAX];
    int in_body = 0;

    if (list_index == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (proxymail_load_inbox(service, hybbx_session_username(session),
                                   entries, HYBBX_MAIL_MAX_MESSAGES,
                                   &count) != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not read proxymail inbox.");
        return HYBBX_ERR_IO;
    }

    if ((size_t)list_index > count) {
        hybbx_session_write_line(session, "No message at that index.");
        return HYBBX_ERR_NOT_FOUND;
    }

    entry = &entries[list_index - 1];

    if (proxymail_user_inbox_path(service, hybbx_session_username(session),
                                  inbox_path, sizeof(inbox_path)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    snprintf(msg_name, sizeof(msg_name), "%06llu.msg",
             (unsigned long long)entry->id);
    if (hybbx_path_join(msg_path, sizeof(msg_path), inbox_path, msg_name) !=
        HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    fp = fopen(msg_path, "r");
    if (fp == NULL) {
        hybbx_session_write_line(session, "Could not open message.");
        return HYBBX_ERR_IO;
    }

    hybbx_session_write_line(session, entry->subject[0] != '\0'
                                       ? entry->subject : "(no subject)");
    hybbx_session_write_line(session, entry->from);

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!in_body) {
            if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
                in_body = 1;
            }
            continue;
        }

        while (line[0] != '\0' &&
               (line[strlen(line) - 1] == '\n' ||
                line[strlen(line) - 1] == '\r')) {
            line[strlen(line) - 1] = '\0';
        }

        hybbx_session_write_line(session, line);
    }

    fclose(fp);
    return HYBBX_OK;
}

hybbx_result_t hybbx_proxymail_delete_range(hybbx_service_t *service,
                                            hybbx_session_t *session,
                                            unsigned from, unsigned to)
{
    proxymail_entry_t entries[HYBBX_MAIL_MAX_MESSAGES];
    size_t count = 0;
    char inbox_path[HYBBX_PATH_MAX];
    size_t i;
    unsigned removed = 0;

    if (proxymail_load_inbox(service, hybbx_session_username(session),
                                   entries, HYBBX_MAIL_MAX_MESSAGES,
                                   &count) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (to == 0 || to > count) {
        to = (unsigned)count;
    }

    if (from == 0 || from > count || from > to) {
        hybbx_session_write_line(session, "Nothing to delete.");
        return HYBBX_OK;
    }

    if (proxymail_user_inbox_path(service, hybbx_session_username(session),
                                  inbox_path, sizeof(inbox_path)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    for (i = (size_t)(from - 1); i < (size_t)to; i++) {
        char msg_path[HYBBX_PATH_MAX];
        char msg_name[32];

        snprintf(msg_name, sizeof(msg_name), "%06llu.msg",
                 (unsigned long long)entries[i].id);
        if (hybbx_path_join(msg_path, sizeof(msg_path), inbox_path,
                            msg_name) == HYBBX_OK) {
            if (remove(msg_path) == 0) {
                removed++;
            }
        }
    }

    if (removed == 0) {
        hybbx_session_write_line(session, "Nothing to delete.");
    } else {
        char buf[48];

        snprintf(buf, sizeof(buf), "Deleted %u proxymail message(s).", removed);
        hybbx_session_write_line(session, buf);
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_proxymail_recycle_empty(hybbx_service_t *service,
                                             hybbx_session_t *session)
{
    (void)service;
    hybbx_session_write_line(session, "Recycle bin empty.");
    return HYBBX_OK;
}

hybbx_result_t hybbx_proxymail_deliver(hybbx_service_t *service,
                                       const char *from_user,
                                       const char *to_address,
                                       const char *subject,
                                       const char *body)
{
    char from_address[HYBBX_PROXYMAIL_ADDRESS_MAX];
    const char *local_name;

    if (!hybbx_proxymail_parse_address(to_address, NULL, 0, NULL, 0)) {
        return HYBBX_ERR_INVALID;
    }

    local_name = hybbx_service_get_name(service);
    snprintf(from_address, sizeof(from_address), "%s@%s",
             from_user != NULL ? from_user : "user", local_name);

    return hybbx_mains_proxy_send_mail(service, from_address, to_address,
                                       subject, body);
}

void hybbx_proxymail_receive(hybbx_service_t *service,
                             const char *from_address,
                             const char *to_address,
                             const char *subject,
                             const char *body)
{
    char user[HYBBX_USER_NAME_MAX];
    char remote[HYBBX_PROXYMAIL_SERVICE_NAME_MAX];
    const char *local_name;

    if (service == NULL || to_address == NULL) {
        return;
    }

    if (!hybbx_proxymail_parse_address(to_address, user, sizeof(user),
                                       remote, sizeof(remote))) {
        return;
    }

    local_name = hybbx_service_get_name(service);
    if (!str_ieq(remote, local_name)) {
        return;
    }

    (void)proxymail_store_message(service, user,
                                        from_address != NULL ? from_address
                                                             : "unknown",
                                        subject, body);
}
