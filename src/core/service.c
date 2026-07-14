/*
 * Centralized daemon: INI apply, plugin lifecycle, HBX circuit hub, sessions,
 * link registry prune. Wire protocols stay in plugins/ (telnet, packet_radio).
 */
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "storage_private.h"
#include "hybbx/registry.h"
#include "hybbx/config.h"
#include "hybbx/crypto_config.h"
#include "hybbx/traffic.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit_bridge.h"
#include "hybbx/link.h"
#include "hybbx/auth.h"
#include "hybbx/storage.h"
#include "hybbx/texts.h"
#include "hybbx/chat.h"
#include "hybbx/mail.h"
#include "hybbx/broadcast.h"

#ifdef HYBBX_HAVE_PLUGIN_MAINS_PROXY
void hybbx_mains_proxy_plugin_tick(void);
#endif
#include "hybbx/networks.h"
#include "hybbx/log.h"
#include "hybbx/security.h"
#include "hybbx/security_ban.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define HYBBX_MAX_ACTIVE_TRANSPORTS 8

static char *hybbx_strdup(const char *s)
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

typedef struct active_transport {
    const hybbx_transport_plugin_t *plugin;
    int running;
} active_transport_t;

typedef struct hybbx_attached_session {
    struct hybbx_session *session;
    struct hybbx_attached_session *next;
} hybbx_attached_session_t;

#define HYBBX_DEFAULT_DATA_PATH "data"

struct hybbx_service_internal {
    char *name;
    char config_path[HYBBX_PATH_MAX];
    char prompt[HYBBX_PROMPT_MAX];
    unsigned max_online;
    unsigned guest_timeout_minutes;
    unsigned active_nodes;
    pthread_mutex_t node_lock;
    pthread_mutex_t session_lock;
    hybbx_attached_session_t *sessions;
    hybbx_storage_t *storage;
    hybbx_auth_config_t auth;
    hybbx_texts_config_t texts;
    hybbx_chat_config_t chat;
    hybbx_mail_config_t mail;
    hybbx_broadcast_config_t broadcast;
    hybbx_networks_config_t networks;
    active_transport_t transports[HYBBX_MAX_ACTIVE_TRANSPORTS];
    size_t transport_count;
    hybbx_circuit_hub_t *circuit_hub;
    int running;
    hybbx_shutdown_mode_t shutdown_mode;
    char launch_binary[HYBBX_PATH_MAX];
    pthread_mutex_t guest_lock;
    unsigned char guest_in_use[HYBBX_GUEST_NUMBER_MAX + 1];
};

hybbx_service_t *hybbx_service_create(const char *name)
{
    struct hybbx_service_internal *svc;

    svc = calloc(1, sizeof(*svc));
    if (svc == NULL) {
        return NULL;
    }

    hybbx_auth_config_defaults(&svc->auth);
    hybbx_texts_config_defaults(&svc->texts);
    hybbx_chat_config_defaults(&svc->chat);
    hybbx_mail_config_defaults(&svc->mail);
    hybbx_networks_config_defaults(&svc->networks);
    svc->max_online = HYBBX_DEFAULT_MAX_ONLINE;
    svc->guest_timeout_minutes = HYBBX_DEFAULT_GUEST_TIMEOUT_MINUTES;
    svc->active_nodes = 0;
    pthread_mutex_init(&svc->node_lock, NULL);
    pthread_mutex_init(&svc->session_lock, NULL);
    pthread_mutex_init(&svc->guest_lock, NULL);

    svc->name = hybbx_strdup(name != NULL && name[0] != '\0' ?
                             name : HYBBX_DEFAULT_SERVICE_NAME);
    if (svc->name == NULL) {
        free(svc);
        return NULL;
    }

    return (hybbx_service_t *)svc;
}

void hybbx_service_destroy(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    size_t i;

    if (svc == NULL) {
        return;
    }

    hybbx_service_stop(service);

    if (svc->circuit_hub != NULL) {
        hybbx_circuit_hub_destroy(svc->circuit_hub);
        svc->circuit_hub = NULL;
    }

    for (i = 0; i < svc->transport_count; i++) {
        if (svc->transports[i].running &&
            svc->transports[i].plugin->stop != NULL) {
            svc->transports[i].plugin->stop();
        }
        if (svc->transports[i].plugin->shutdown != NULL) {
            svc->transports[i].plugin->shutdown();
        }
    }

    if (svc->storage != NULL) {
        hybbx_storage_close(svc->storage);
        svc->storage = NULL;
    }

    hybbx_log_shutdown();
    hybbx_security_log_shutdown();
    hybbx_security_ban_shutdown();

    {
        hybbx_attached_session_t *node = svc->sessions;

        while (node != NULL) {
            hybbx_attached_session_t *next = node->next;

            free(node);
            node = next;
        }
        svc->sessions = NULL;
    }

    pthread_mutex_destroy(&svc->session_lock);
    pthread_mutex_destroy(&svc->guest_lock);
    pthread_mutex_destroy(&svc->node_lock);
    free(svc->name);
    free(svc);
}

hybbx_storage_t *hybbx_service_get_storage(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return svc->storage;
}

const hybbx_auth_config_t *hybbx_service_get_auth(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return &svc->auth;
}

const hybbx_texts_config_t *hybbx_service_get_texts(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return &svc->texts;
}

const hybbx_chat_config_t *hybbx_service_get_chat(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return &svc->chat;
}

const hybbx_mail_config_t *hybbx_service_get_mail(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return &svc->mail;
}

const hybbx_broadcast_config_t *hybbx_service_get_broadcast(
    const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return &svc->broadcast;
}

hybbx_circuit_hub_t *hybbx_service_circuit_hub(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return NULL;
    }

    return svc->circuit_hub;
}

const char *hybbx_service_get_prompt(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return "";
    }

    return svc->prompt;
}

const char *hybbx_service_get_name(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL || svc->name == NULL || svc->name[0] == '\0') {
        return HYBBX_DEFAULT_SERVICE_NAME;
    }

    return svc->name;
}

unsigned hybbx_service_max_online(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return HYBBX_DEFAULT_MAX_ONLINE;
    }

    return svc->max_online;
}

unsigned hybbx_service_active_nodes(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;
    unsigned count;

    if (svc == NULL) {
        return 0;
    }

    pthread_mutex_lock((pthread_mutex_t *)&svc->node_lock);
    count = svc->active_nodes;
    pthread_mutex_unlock((pthread_mutex_t *)&svc->node_lock);

    return count;
}

unsigned hybbx_service_guest_timeout_seconds(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return HYBBX_DEFAULT_GUEST_TIMEOUT_MINUTES * 60u;
    }

    return svc->guest_timeout_minutes * 60u;
}

hybbx_result_t hybbx_service_guest_assign(hybbx_service_t *service,
                                          const char *guest_prefix,
                                          hybbx_user_record_t *out,
                                          unsigned *slot_out)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    unsigned slot;

    if (svc == NULL || out == NULL || slot_out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    pthread_mutex_lock(&svc->guest_lock);
    for (slot = 1; slot <= HYBBX_GUEST_NUMBER_MAX; slot++) {
        if (!svc->guest_in_use[slot]) {
            svc->guest_in_use[slot] = 1;
            pthread_mutex_unlock(&svc->guest_lock);

            hybbx_guest_fill_record(guest_prefix, slot, out);
            *slot_out = slot;
            return HYBBX_OK;
        }
    }
    pthread_mutex_unlock(&svc->guest_lock);

    return HYBBX_ERR_BUSY;
}

void hybbx_service_guest_release(hybbx_service_t *service, unsigned slot)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL || slot < 1 || slot > HYBBX_GUEST_NUMBER_MAX) {
        return;
    }

    pthread_mutex_lock(&svc->guest_lock);
    svc->guest_in_use[slot] = 0;
    pthread_mutex_unlock(&svc->guest_lock);
}

hybbx_result_t hybbx_service_acquire_node(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return HYBBX_ERR_INVALID;
    }

    pthread_mutex_lock(&svc->node_lock);
    if (svc->active_nodes >= svc->max_online) {
        pthread_mutex_unlock(&svc->node_lock);
        return HYBBX_ERR_BUSY;
    }

    svc->active_nodes++;
    pthread_mutex_unlock(&svc->node_lock);
    return HYBBX_OK;
}

void hybbx_service_release_node(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return;
    }

    pthread_mutex_lock(&svc->node_lock);
    if (svc->active_nodes > 0) {
        svc->active_nodes--;
    }
    pthread_mutex_unlock(&svc->node_lock);
}

hybbx_result_t hybbx_service_attach_session(hybbx_service_t *service,
                                            hybbx_session_t *session)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    hybbx_attached_session_t *node;

    if (svc == NULL || session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    node->session = session;

    pthread_mutex_lock(&svc->session_lock);
    node->next = svc->sessions;
    svc->sessions = node;
    pthread_mutex_unlock(&svc->session_lock);

    return HYBBX_OK;
}

void hybbx_service_detach_session(hybbx_service_t *service,
                                  hybbx_session_t *session)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    hybbx_attached_session_t *prev;
    hybbx_attached_session_t *node;

    if (svc == NULL || session == NULL) {
        return;
    }

    pthread_mutex_lock(&svc->session_lock);
    prev = NULL;
    node = svc->sessions;
    while (node != NULL) {
        if (node->session == session) {
            if (prev != NULL) {
                prev->next = node->next;
            } else {
                svc->sessions = node->next;
            }
            free(node);
            break;
        }
        prev = node;
        node = node->next;
    }
    pthread_mutex_unlock(&svc->session_lock);
}

void hybbx_service_visit_sessions(hybbx_service_t *service,
                                  hybbx_service_session_visit_fn fn,
                                  void *userdata)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    hybbx_attached_session_t *node;
    hybbx_session_t **snapshot = NULL;
    size_t count = 0;
    size_t i;

    if (svc == NULL || fn == NULL) {
        return;
    }

    /*
     * Snapshot session pointers under session_lock, then invoke callbacks
     * without holding the lock. Visitors may write to circuit sessions,
     * which can re-enter via load-balance -> bandwidth policy.
     */
    pthread_mutex_lock(&svc->session_lock);
    for (node = svc->sessions; node != NULL; node = node->next) {
        if (node->session != NULL) {
            count++;
        }
    }

    if (count > 0) {
        snapshot = calloc(count, sizeof(*snapshot));
        if (snapshot == NULL) {
            hybbx_log_warn("[service] session snapshot OOM (%zu sessions) — skipped",
                           count);
        } else {
            i = 0;
            for (node = svc->sessions; node != NULL; node = node->next) {
                if (node->session != NULL) {
                    snapshot[i++] = node->session;
                }
            }
        }
    }
    pthread_mutex_unlock(&svc->session_lock);

    if (snapshot == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        fn(snapshot[i], userdata);
    }
    free(snapshot);
}

typedef struct find_registered_session_ctx {
    uint64_t user_id;
    hybbx_session_t *exclude;
    hybbx_session_t *found;
} find_registered_session_ctx_t;

static void find_registered_session_visitor(hybbx_session_t *session,
                                            void *userdata)
{
    find_registered_session_ctx_t *ctx = (find_registered_session_ctx_t *)userdata;
    const hybbx_session_record_t *rec;

    if (ctx == NULL || ctx->found != NULL || session == NULL ||
        session == ctx->exclude) {
        return;
    }

    if (!hybbx_session_logged_in(session) || hybbx_session_is_guest(session)) {
        return;
    }

    rec = hybbx_session_record(session);
    if (rec == NULL || rec->user_id != ctx->user_id) {
        return;
    }

    ctx->found = session;
}

hybbx_session_t *hybbx_service_find_registered_session(
    hybbx_service_t *service,
    uint64_t user_id,
    hybbx_session_t *exclude)
{
    find_registered_session_ctx_t ctx;

    if (service == NULL || user_id == 0) {
        return NULL;
    }

    ctx.user_id = user_id;
    ctx.exclude = exclude;
    ctx.found = NULL;
    hybbx_service_visit_sessions(service, find_registered_session_visitor, &ctx);
    return ctx.found;
}

static void service_apply_service(struct hybbx_service_internal *svc,
                                  const hybbx_config_t *config)
{
    const char *prompt;
    const char *max_online_value;
    const char *nodes_value;

    svc->prompt[0] = '\0';

    prompt = hybbx_config_get(config, "service", "prompt", NULL);
    if (prompt != NULL && prompt[0] != '\0') {
        hybbx_strlcpy(svc->prompt, prompt, sizeof(svc->prompt));
    }

    max_online_value = hybbx_config_get(config, "service", "max_online", NULL);
    nodes_value = hybbx_config_get(config, "service", "nodes", NULL);
    if (max_online_value != NULL && max_online_value[0] != '\0') {
        svc->max_online = hybbx_config_get_uint(config, "service", "max_online",
                                                HYBBX_DEFAULT_MAX_ONLINE, 1u,
                                                999u);
    } else if (nodes_value != NULL && nodes_value[0] != '\0') {
        svc->max_online = hybbx_config_get_uint(config, "service", "nodes",
                                                HYBBX_DEFAULT_MAX_ONLINE, 1u,
                                                999u);
    } else {
        svc->max_online = HYBBX_DEFAULT_MAX_ONLINE;
    }

    hybbx_log_info("[service] max_online=%u", svc->max_online);
}

static int str_ieq_local(const char *a, const char *b)
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

static void service_apply_texts(struct hybbx_service_internal *svc,
                                const hybbx_config_t *config)
{
    const char *path;
    char resolved[HYBBX_PATH_MAX];

    hybbx_texts_config_defaults(&svc->texts);
    path = hybbx_config_get(config, "texts", "path", HYBBX_DIR_TEXT);
    if (hybbx_path_resolve(resolved, sizeof(resolved), path) == HYBBX_OK) {
        hybbx_strlcpy(svc->texts.path, resolved, sizeof(svc->texts.path));
    } else if (path != NULL && path[0] != '\0') {
        hybbx_strlcpy(svc->texts.path, path, sizeof(svc->texts.path));
    }
}

static hybbx_storage_backend_kind_t parse_storage_backend(const char *value)
{
    if (value == NULL || str_ieq_local(value, "flatfile") ||
        str_ieq_local(value, "flat") || str_ieq_local(value, "files")) {
        return HYBBX_STORAGE_FLATFILE;
    }

    if (str_ieq_local(value, "sqlite")) {
        return HYBBX_STORAGE_SQLITE;
    }

    if (str_ieq_local(value, "mysql")) {
        return HYBBX_STORAGE_MYSQL;
    }

    if (str_ieq_local(value, "mariadb")) {
        return HYBBX_STORAGE_MARIADB;
    }

    return HYBBX_STORAGE_FLATFILE;
}

static hybbx_result_t service_open_storage(struct hybbx_service_internal *svc,
                                           const hybbx_config_t *config)
{
    hybbx_storage_options_t options;
    hybbx_storage_sql_config_t sql_cfg;
    const char *backend_str;
    const char *path_raw;
    char path_resolved[HYBBX_PATH_MAX];
    const char *guest_prefix;
    hybbx_result_t rc;

    if (svc->storage != NULL) {
        hybbx_storage_close(svc->storage);
        svc->storage = NULL;
    }

    backend_str = hybbx_config_get(config, "storage", "backend", "flatfile");
    path_raw = hybbx_config_get(config, "storage", "path", HYBBX_DEFAULT_DATA_PATH);

    rc = hybbx_path_resolve(path_resolved, sizeof(path_resolved), path_raw);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[storage] invalid path '%s'",
                path_raw != NULL ? path_raw : "");
        return HYBBX_ERR_IO;
    }

    guest_prefix = hybbx_config_get(config, "auth", "guest_prefix", NULL);

    options.backend = parse_storage_backend(backend_str);
    options.path = path_resolved;
    options.guest_prefix = guest_prefix != NULL ? guest_prefix :
                                                  svc->auth.guest_prefix;
    options.sql_cfg = NULL;

    if (options.backend == HYBBX_STORAGE_SQLITE) {
        hybbx_storage_sql_config_apply(&sql_cfg, config, path_resolved);
        options.sql_cfg = &sql_cfg;
    }

    svc->storage = hybbx_storage_open(&options);
    if (svc->storage == NULL) {
        hybbx_log_warn("[storage] cannot open '%s' (writable?)",
                path_resolved);
        return HYBBX_ERR_IO;
    }

    hybbx_log_info("[storage] backend=%s path=%s", backend_str, path_resolved);
    return HYBBX_OK;
}

static void service_apply_auth(struct hybbx_service_internal *svc,
                               const hybbx_config_t *config)
{
    const char *prefix;

    hybbx_auth_config_defaults(&svc->auth);
    svc->auth.auto_login =
        hybbx_config_get_bool(config, "auth", "auto_login", 1);

    prefix = hybbx_config_get(config, "auth", "guest_prefix", NULL);
    if (prefix != NULL && prefix[0] != '\0') {
        hybbx_strlcpy(svc->auth.guest_prefix, prefix,
                      sizeof(svc->auth.guest_prefix));
    }

    svc->guest_timeout_minutes = hybbx_config_get_uint(
        config, "auth", "guest_timeout_minutes",
        HYBBX_DEFAULT_GUEST_TIMEOUT_MINUTES, 1u, 24u * 60u);

    hybbx_log_info("[service] guest_timeout_minutes=%u", svc->guest_timeout_minutes);
}

static hybbx_result_t service_apply_circuit(struct hybbx_service_internal *svc,
                                            const hybbx_config_t *config)
{
    hybbx_circuit_config_t cfg;
    const char *bind4;
    const char *bind6;
    hybbx_result_t rc;

    if (!hybbx_config_get_bool(config, "circuit", "enabled", 1)) {
        if (svc->circuit_hub != NULL) {
            hybbx_circuit_hub_stop(svc->circuit_hub);
        }
        return HYBBX_OK;
    }

    if (!svc->networks.circuit) {
        if (svc->circuit_hub != NULL) {
            hybbx_circuit_hub_stop(svc->circuit_hub);
        }
        return HYBBX_OK;
    }

    hybbx_circuit_config_defaults(&cfg);
    bind4 = hybbx_config_get(config, "circuit", "bind", NULL);
    bind6 = hybbx_config_get(config, "circuit", "bind6", NULL);
    if (bind4 != NULL && bind4[0] != '\0') {
        hybbx_strlcpy(cfg.bind4, bind4, sizeof(cfg.bind4));
    }
    if (bind6 != NULL && bind6[0] != '\0') {
        hybbx_strlcpy(cfg.bind6, bind6, sizeof(cfg.bind6));
    }

    cfg.port = hybbx_config_get_uint(config, "circuit", "port",
                                     HYBBX_CIRCUIT_DEFAULT_PORT, 1u, 65535u);
    cfg.ipv4 = hybbx_config_get_bool(config, "circuit", "ipv4", 1);
    cfg.ipv6 = hybbx_config_get_bool(config, "circuit", "ipv6", 1);
    cfg.link_auth = hybbx_config_get_bool(config, "circuit", "link_auth", 1);
    cfg.link_stale_days = hybbx_config_get_uint(
        config, "circuit", "link_stale_days", HYBBX_LINK_STALE_DAYS, 1u, 365u);
    cfg.balance.enabled = hybbx_config_get_bool(config, "circuit", "balance", 1);
    cfg.balance.lag_sec = hybbx_config_get_uint(
        config, "circuit", "balance_lag_sec", 8u, 1u, 120u);
    cfg.balance.queue_pause = (size_t)hybbx_config_get_uint(
        config, "circuit", "balance_queue_pause", 8192u, 256u, 1048576u);
    cfg.balance.queue_break = (size_t)hybbx_config_get_uint(
        config, "circuit", "balance_queue_break", 32768u, 1024u, 1048576u);
    cfg.balance.queue_cancel = (size_t)hybbx_config_get_uint(
        config, "circuit", "balance_queue_cancel", 131072u, 4096u, 4194304u);
    if (cfg.balance.queue_pause > cfg.balance.queue_break) {
        cfg.balance.queue_break = cfg.balance.queue_pause;
    }
    if (cfg.balance.queue_break > cfg.balance.queue_cancel) {
        cfg.balance.queue_cancel = cfg.balance.queue_break;
    }
    cfg.max_links = hybbx_config_get_uint(
        config, "circuit", "max_links", HYBBX_CIRCUIT_DEFAULT_MAX_LINKS,
        1u, HYBBX_CIRCUIT_MAX_LINKS);
    (void)hybbx_circuit_bridge_load(&cfg.bridge, config);

    {
        const char *link_pw = hybbx_config_get(config, "circuit", "link_password", NULL);

        if (link_pw != NULL) {
            hybbx_strlcpy(cfg.link_password, link_pw, sizeof(cfg.link_password));
        }
        if (svc->storage != NULL && svc->storage->path != NULL &&
            svc->storage->path[0] != '\0') {
            hybbx_strlcpy(cfg.data_path, svc->storage->path, sizeof(cfg.data_path));
        } else {
            char data_resolved[HYBBX_PATH_MAX];
            const char *path_raw = hybbx_config_get(config, "storage", "path",
                                                    HYBBX_DEFAULT_DATA_PATH);

            if (hybbx_path_resolve(data_resolved, sizeof(data_resolved),
                                   path_raw) == HYBBX_OK) {
                hybbx_strlcpy(cfg.data_path, data_resolved, sizeof(cfg.data_path));
            }
        }
        if (svc->config_path[0] != '\0') {
            hybbx_strlcpy(cfg.config_path, svc->config_path, sizeof(cfg.config_path));
        }
    }

    if (svc->circuit_hub == NULL) {
        svc->circuit_hub = hybbx_circuit_hub_create((hybbx_service_t *)svc);
        if (svc->circuit_hub == NULL) {
            return HYBBX_ERR_NOMEM;
        }
    }

    rc = hybbx_circuit_hub_start(svc->circuit_hub, &cfg);
    return rc;
}

hybbx_result_t hybbx_service_load_transport(hybbx_service_t *service,
                                            const char *plugin_name)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    const hybbx_transport_plugin_t *plugin;
    size_t i;

    if (svc == NULL || plugin_name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    plugin = hybbx_registry_find(plugin_name);
    if (plugin == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    for (i = 0; i < svc->transport_count; i++) {
        if (svc->transports[i].plugin == plugin) {
            return HYBBX_OK;
        }
    }

    if (svc->transport_count >= HYBBX_MAX_ACTIVE_TRANSPORTS) {
        return HYBBX_ERR_NOMEM;
    }

    if (plugin->init != NULL) {
        hybbx_result_t rc = plugin->init(service);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    svc->transports[svc->transport_count].plugin = plugin;
    svc->transports[svc->transport_count].running = 0;
    svc->transport_count++;
    return HYBBX_OK;
}

static active_transport_t *find_active(struct hybbx_service_internal *svc,
                                       const char *plugin_name)
{
    size_t i;

    for (i = 0; i < svc->transport_count; i++) {
        if (strcmp(svc->transports[i].plugin->name, plugin_name) == 0) {
            return &svc->transports[i];
        }
    }
    return NULL;
}

hybbx_result_t hybbx_service_start_transport(hybbx_service_t *service,
                                               const char *plugin_name,
                                               const char *config)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    active_transport_t *active;

    if (svc == NULL || plugin_name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    active = find_active(svc, plugin_name);
    if (active == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    if (active->running) {
        return HYBBX_ERR_BUSY;
    }

    if (active->plugin->start == NULL) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    {
        hybbx_result_t rc = active->plugin->start(config);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    active->running = 1;
    return HYBBX_OK;
}

hybbx_result_t hybbx_service_stop_transport(hybbx_service_t *service,
                                              const char *plugin_name)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    active_transport_t *active;

    if (svc == NULL || plugin_name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    active = find_active(svc, plugin_name);
    if (active == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    if (!active->running) {
        return HYBBX_OK;
    }

    if (active->plugin->stop != NULL) {
        hybbx_result_t rc = active->plugin->stop();
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    active->running = 0;
    return HYBBX_OK;
}

hybbx_result_t hybbx_service_run(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return HYBBX_ERR_INVALID;
    }

    svc->running = 1;

    while (svc->running) {
#if defined(_WIN32)
        Sleep(1000);
#else
        static unsigned prune_tick;

        sleep(1);
        hybbx_security_ban_tick();
        hybbx_broadcast_ax25_tick(service);
#ifdef HYBBX_HAVE_PLUGIN_MAINS_PROXY
        hybbx_mains_proxy_plugin_tick();
#endif
        if (svc->storage != NULL) {
            hybbx_storage_backup_tick(svc->storage);
        }
        prune_tick++;
        if (prune_tick >= 3600u) {
            prune_tick = 0;
            if (svc->circuit_hub != NULL) {
                hybbx_circuit_hub_prune_links(svc->circuit_hub);
            }
        }
#endif
    }

    return HYBBX_OK;
}

void hybbx_service_stop(hybbx_service_t *service)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc != NULL) {
        svc->running = 0;
    }
}

void hybbx_service_set_launch_binary(hybbx_service_t *service, const char *argv0)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return;
    }

    svc->launch_binary[0] = '\0';
    if (argv0 != NULL && argv0[0] != '\0') {
        hybbx_strlcpy(svc->launch_binary, argv0, sizeof(svc->launch_binary));
    }
}

void hybbx_service_request_shutdown(hybbx_service_t *service, int restart)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return;
    }

    svc->shutdown_mode = restart ? HYBBX_SHUTDOWN_RESTART : HYBBX_SHUTDOWN_STOP;
    hybbx_service_stop(service);
}

hybbx_shutdown_mode_t hybbx_service_shutdown_mode(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL) {
        return HYBBX_SHUTDOWN_NONE;
    }

    return svc->shutdown_mode;
}

const char *hybbx_service_config_path(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;

    if (svc == NULL || svc->config_path[0] == '\0') {
        return NULL;
    }

    return svc->config_path;
}

void hybbx_service_restart_exec(const hybbx_service_t *service)
{
    const struct hybbx_service_internal *svc =
        (const struct hybbx_service_internal *)service;
    const char *binary;
    const char *config_path;
    char *argv[4];
    char opt_c[] = "-c";

    if (svc == NULL) {
        return;
    }

    binary = svc->launch_binary[0] != '\0' ? svc->launch_binary : HYBBX_DAEMON_BINARY;
    config_path = svc->config_path[0] != '\0' ? svc->config_path : NULL;

    argv[0] = (char *)binary;
    if (config_path != NULL) {
        argv[1] = opt_c;
        argv[2] = (char *)config_path;
        argv[3] = NULL;
    } else {
        argv[1] = NULL;
    }

    fflush(NULL);
    execv(binary, argv);
    perror("hybbx restart failed");
}

typedef struct apply_transport_ctx {
    hybbx_service_t *service;
    const hybbx_config_t *config;
    const hybbx_networks_config_t *networks;
    hybbx_result_t last_error;
} apply_transport_ctx_t;

static int transport_start_failure_is_fatal(const char *plugin_name)
{
    /* Telnet is the primary session transport; others may fail independently. */
    return plugin_name != NULL && strcmp(plugin_name, "telnet") == 0;
}

static void apply_transport_cb(const hybbx_transport_plugin_t *plugin,
                               void *userdata)
{
    apply_transport_ctx_t *ctx = (apply_transport_ctx_t *)userdata;
    char section[128];
    char *transport_config;
    hybbx_result_t rc;
    int transport_enabled;

    if (ctx->networks == NULL ||
        !hybbx_networks_transport_wanted(plugin->name, ctx->networks)) {
        return;
    }

    if (!hybbx_config_resolve_transport_section(ctx->config, plugin->name,
                                                section, sizeof(section))) {
        snprintf(section, sizeof(section), "transport.%s", plugin->name);
    }

    if (hybbx_networks_is_static_transport(plugin->name)) {
        transport_enabled = 1;
    } else {
        transport_enabled = hybbx_config_get_bool(ctx->config, section,
                                                  "enabled", 1);
    }

    if (!transport_enabled) {
        return;
    }

    rc = hybbx_service_load_transport(ctx->service, plugin->name);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[service] %s: plugin load failed (%s)",
                plugin->name, hybbx_result_name(rc));
        if (transport_start_failure_is_fatal(plugin->name)) {
            ctx->last_error = rc;
        }
        return;
    }

    if (strcmp(plugin->name, "packet_radio") == 0) {
        transport_config =
            hybbx_config_format_packet_radio_start(ctx->config);
    } else if (strcmp(plugin->name, "baycom") == 0 ||
               strcmp(plugin->name, "mains_proxy") == 0) {
        transport_config = hybbx_config_format_transport_sections(ctx->config,
                                                                  plugin->name);
    } else {
        transport_config = NULL;
    }
    if (transport_config == NULL) {
        transport_config = hybbx_config_format_section(ctx->config, section);
    }
    rc = hybbx_service_start_transport(ctx->service, plugin->name,
                                       transport_config);
    free(transport_config);

    if (rc != HYBBX_OK) {
        hybbx_log_warn("[service] %s: start failed (%s)", plugin->name,
                hybbx_result_name(rc));
        if (transport_start_failure_is_fatal(plugin->name)) {
            ctx->last_error = rc;
        }
    }
}

hybbx_result_t hybbx_service_apply_config(hybbx_service_t *service,
                                            const hybbx_config_t *config,
                                            const char *config_path)
{
    struct hybbx_service_internal *svc =
        (struct hybbx_service_internal *)service;
    const char *service_name;
    apply_transport_ctx_t ctx;
    char *new_name;
    hybbx_result_t rc;

    if (svc == NULL || config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    svc->config_path[0] = '\0';
    if (config_path != NULL && config_path[0] != '\0') {
        hybbx_strlcpy(svc->config_path, config_path, sizeof(svc->config_path));
    }

    service_name = hybbx_config_get(config, "service", "name", NULL);
    if (service_name != NULL) {
        new_name = hybbx_strdup(service_name);
        if (new_name == NULL) {
            return HYBBX_ERR_NOMEM;
        }
        free(svc->name);
        svc->name = new_name;
    }

    service_apply_auth(svc, config);
    hybbx_time_config_apply(config);
    hybbx_log_config_apply(config);
    hybbx_security_log_config_apply(config);
    hybbx_security_ban_config_apply(config);
    service_apply_texts(svc, config);
    hybbx_chat_config_apply(&svc->chat, config);
    hybbx_broadcast_config_apply(&svc->broadcast, config);
    service_apply_service(svc, config);
    hybbx_crypto_config_apply(config);
    hybbx_traffic_config_apply(config);

    rc = service_open_storage(svc, config);
    if (rc != HYBBX_OK) {
        return rc;
    }

    {
        const char *storage_path = HYBBX_DEFAULT_DATA_PATH;
        char storage_resolved[HYBBX_PATH_MAX];

        if (svc->storage != NULL && svc->storage->path != NULL &&
            svc->storage->path[0] != '\0') {
            storage_path = svc->storage->path;
        } else if (hybbx_path_resolve(storage_resolved, sizeof(storage_resolved),
                                      storage_path) == HYBBX_OK) {
            storage_path = storage_resolved;
        }
        hybbx_mail_config_apply(&svc->mail, config, storage_path);
    }

    hybbx_networks_config_apply(&svc->networks, config);

    rc = service_apply_circuit(svc, config);
    if (rc != HYBBX_OK) {
        return rc;
    }

    ctx.service = service;
    ctx.config = config;
    ctx.networks = &svc->networks;
    ctx.last_error = HYBBX_OK;

    hybbx_registry_foreach(apply_transport_cb, &ctx);
    return ctx.last_error;
}
