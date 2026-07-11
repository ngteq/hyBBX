#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/circuit_tcp.h"
#include "hybbx/mains_proxy.h"
#include "hybbx/circuit.h"
#include "hybbx/broadcast.h"
#include "hybbx/circuit_balance.h"
#include "hybbx/circuit_bridge.h"
#include "hybbx/bandwidth_policy.h"
#include "hybbx/session.h"
#include "hybbx/service.h"
#include "hybbx/security.h"
#include "hybbx/security_ban.h"
#include "hybbx/storage.h"
#include "hybbx/traffic.h"
#include "hybbx/link.h"
#include "hybbx/password.h"
#include "hybbx/socket.h"
#include "hybbx/util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HYBBX_CIRCUIT_LINK_POLL_MS 50
#define HYBBX_CIRCUIT_AUTH_TIMEOUT_MS 15000

typedef enum {
    CIRCUIT_SLOT_FREE = 0,
    CIRCUIT_SLOT_CONNECTING,
    CIRCUIT_SLOT_ACTIVE
} circuit_slot_state_t;

typedef struct hybbx_circuit_link_slot {
    hybbx_circuit_hub_t *hub;
    circuit_slot_state_t state;
    int fd;
    pthread_t thread;
    char link_id[HYBBX_LINK_ID_MAX];
    hybbx_session_t *session;
    hybbx_circuit_decoder_t decoder;
    hybbx_circuit_balance_t *balance;
    hybbx_circuit_link_profile_t profile;
    int profile_set;
} hybbx_circuit_link_slot_t;

struct hybbx_circuit_hub {
    hybbx_service_t *service;
    hybbx_circuit_config_t config;
    hybbx_link_registry_t links;
    hybbx_circuit_bridge_registry_t bridge;
    unsigned max_links;
    char link_password[128];
    int link_auth;
    int listen_v4;
    int listen_v6;
    pthread_t accept_thread;
    pthread_mutex_t lock;
    volatile int running;
    hybbx_circuit_link_slot_t slots[HYBBX_CIRCUIT_MAX_LINKS];
};

void hybbx_circuit_config_defaults(hybbx_circuit_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->bind4, sizeof(cfg->bind4), "127.0.0.1");
    snprintf(cfg->bind6, sizeof(cfg->bind6), "::1");
    cfg->port = HYBBX_CIRCUIT_DEFAULT_PORT;
    cfg->ipv4 = 1;
    cfg->ipv6 = 1;
    cfg->link_stale_days = HYBBX_LINK_STALE_DAYS;
    cfg->link_auth = 1;
    hybbx_circuit_balance_config_defaults(&cfg->balance);
    cfg->max_links = HYBBX_CIRCUIT_DEFAULT_MAX_LINKS;
    hybbx_circuit_bridge_clear(&cfg->bridge);
}

static int set_socket_options(int fd, int family)
{
    int on = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
        return -1;
    }

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) != 0) {
        return -1;
    }

    hybbx_socket_nosigpipe(fd);

#ifdef IPV6_V6ONLY
    if (family == AF_INET6) {
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) != 0) {
            return -1;
        }
    }
#else
    (void)family;
#endif

    return 0;
}

static int create_listen_socket(int family, const char *bind_addr, unsigned port,
                                int backlog)
{
    int fd;
    int rc;

    fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (set_socket_options(fd, family) != 0) {
        close(fd);
        return -1;
    }

    if (family == AF_INET6) {
        struct sockaddr_in6 addr6;

        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((uint16_t)port);

        if (inet_pton(AF_INET6, bind_addr, &addr6.sin6_addr) != 1) {
            close(fd);
            return -1;
        }

        rc = bind(fd, (struct sockaddr *)&addr6, sizeof(addr6));
    } else {
        struct sockaddr_in addr4;

        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons((uint16_t)port);

        if (inet_pton(AF_INET, bind_addr, &addr4.sin_addr) != 1) {
            close(fd);
            return -1;
        }

        rc = bind(fd, (struct sockaddr *)&addr4, sizeof(addr4));
    }

    if (rc != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, backlog > 0 ? backlog : 8) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int circuit_slot_broadcast_qos(const hybbx_circuit_link_slot_t *slot)
{
    if (slot == NULL || !slot->profile_set) {
        return 0;
    }

    return slot->profile.bandwidth == HYBBX_CIRCUIT_BW_LOW &&
           slot->profile.duplex == HYBBX_CIRCUIT_DUPLEX_HALF;
}

static int circuit_frame_is_ax25_ui_tx(const uint8_t *frame, size_t len)
{
    unsigned flags;

    if (frame == NULL || len < HYBBX_CIRCUIT_HEADER_SIZE) {
        return 0;
    }

    if (frame[0] != HYBBX_CIRCUIT_MAGIC_0 ||
        frame[1] != HYBBX_CIRCUIT_MAGIC_1 ||
        frame[2] != HYBBX_CIRCUIT_MAGIC_2 ||
        frame[3] != HYBBX_CIRCUIT_VERSION ||
        frame[4] != (uint8_t)HYBBX_CIRCUIT_PROTO_AX25_UI) {
        return 0;
    }

    flags = ((unsigned)frame[5] << 8u) | (unsigned)frame[6];
    return (flags & HYBBX_CIRCUIT_FLAG_TX) != 0;
}

static int circuit_slot_can_send_low_prio(const hybbx_circuit_hub_t *hub,
                                          const hybbx_circuit_link_slot_t *slot)
{
    hybbx_circuit_balance_action_t action;
    size_t queued;
    size_t queue_pause;
    size_t reserve_threshold;

    if (hub == NULL || slot == NULL || slot->balance == NULL ||
        !hub->config.balance.enabled || !slot->profile_set) {
        return 1;
    }

    action = hybbx_circuit_balance_action(slot->balance);
    if (action == HYBBX_CIRCUIT_BAL_PAUSE ||
        action == HYBBX_CIRCUIT_BAL_BREAK ||
        action == HYBBX_CIRCUIT_BAL_CANCEL) {
        return 0;
    }

    queued = hybbx_circuit_balance_queued_bytes(slot->balance);
    queue_pause = hub->config.balance.queue_pause;
    if (queue_pause == 0) {
        queue_pause = 1;
    }
    reserve_threshold = queue_pause / 2u;
    if (reserve_threshold == 0) {
        reserve_threshold = 1;
    }

    /* Keep a reserve for interactive user/mesh traffic. */
    return queued < reserve_threshold;
}

static unsigned circuit_count_used_slots(const hybbx_circuit_hub_t *hub)
{
    unsigned i;
    unsigned count = 0;

    if (hub == NULL) {
        return 0;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state != CIRCUIT_SLOT_FREE) {
            count++;
        }
    }

    return count;
}

static int circuit_find_free_slot(hybbx_circuit_hub_t *hub)
{
    unsigned i;

    if (hub == NULL) {
        return -1;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state == CIRCUIT_SLOT_FREE && hub->slots[i].fd < 0) {
            return (int)i;
        }
    }

    return -1;
}

static int circuit_find_active_slot_by_id(const hybbx_circuit_hub_t *hub,
                                          const char *link_id,
                                          const hybbx_circuit_link_slot_t *except)
{
    unsigned i;

    if (hub == NULL || link_id == NULL || link_id[0] == '\0') {
        return -1;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        const hybbx_circuit_link_slot_t *slot = &hub->slots[i];

        if (slot == except) {
            continue;
        }
        if (slot->state == CIRCUIT_SLOT_FREE || slot->link_id[0] == '\0') {
            continue;
        }
        if (strcmp(slot->link_id, link_id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

typedef struct balance_send_ctx {
    hybbx_circuit_link_slot_t *slot;
} balance_send_ctx_t;

typedef struct flow_ctrl_ctx {
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_link_slot_t *slot;
} flow_ctrl_ctx_t;

static hybbx_result_t circuit_slot_send_raw(hybbx_circuit_link_slot_t *slot,
                                            const uint8_t *frame, size_t len)
{
    hybbx_circuit_hub_t *hub;
    ssize_t sent;
    size_t off = 0;
    int fd;

    if (slot == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    hub = slot->hub;
    if (hub == NULL) {
        return HYBBX_ERR_INVALID;
    }

    pthread_mutex_lock(&hub->lock);
    fd = slot->fd;
    if (fd < 0) {
        pthread_mutex_unlock(&hub->lock);
        return HYBBX_ERR_BUSY;
    }

    while (off < len) {
        sent = send(fd, frame + off, len - off, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            pthread_mutex_unlock(&hub->lock);
            return HYBBX_ERR_IO;
        }
        if (sent == 0) {
            pthread_mutex_unlock(&hub->lock);
            return HYBBX_ERR_IO;
        }
        off += (size_t)sent;
    }

    pthread_mutex_unlock(&hub->lock);
    return HYBBX_OK;
}

static hybbx_result_t balance_send_raw_cb(void *ctx, const uint8_t *frame,
                                          size_t len)
{
    balance_send_ctx_t *bctx = (balance_send_ctx_t *)ctx;

    if (bctx == NULL || bctx->slot == NULL) {
        return HYBBX_ERR_INVALID;
    }

    return circuit_slot_send_raw(bctx->slot, frame, len);
}

static int circuit_bandwidth_spare_link(hybbx_circuit_hub_t *hub,
                                        hybbx_circuit_link_slot_t *slot,
                                        hybbx_circuit_balance_action_t action)
{
    unsigned users_before;
    unsigned affected;

    if (hub == NULL || hub->service == NULL || slot == NULL) {
        return 0;
    }

    users_before = hybbx_bandwidth_policy_user_count(hub->service);
    if (users_before == 0) {
        return 0;
    }

    affected = hybbx_bandwidth_policy_apply(hub->service, action);
    if (affected == 0) {
        return 0;
    }

    if (slot->balance != NULL &&
        hybbx_circuit_balance_action(slot->balance) == HYBBX_CIRCUIT_BAL_CANCEL) {
        hybbx_circuit_balance_spared_cancel(slot->balance);
        printf("[circuit] secondary link %s spared — users sacrificed first\n",
               slot->link_id[0] != '\0' ? slot->link_id : "?");
    }

    return 1;
}

static void balance_flow_ctrl_cb(void *ctx,
                                 hybbx_circuit_balance_action_t action,
                                 const char *reason)
{
    flow_ctrl_ctx_t *fctx = (flow_ctrl_ctx_t *)ctx;
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_link_slot_t *slot;
    char payload[128];
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t payload_len;
    size_t frame_len;
    const char *reason_str = reason != NULL ? reason : "-";

    if (fctx == NULL || fctx->hub == NULL || fctx->slot == NULL ||
        action == HYBBX_CIRCUIT_BAL_NONE) {
        return;
    }

    hub = fctx->hub;
    slot = fctx->slot;

    payload_len = hybbx_circuit_flow_ctrl_format(action, reason_str,
                                                 payload, sizeof(payload));
    if (payload_len == 0) {
        return;
    }

    frame_len = hybbx_circuit_encode_link_msg(HYBBX_CIRCUIT_PROTO_FLOW_CTRL,
                                              payload, payload_len,
                                              frame, sizeof(frame));
    if (frame_len > 0) {
        (void)circuit_slot_send_raw(slot, frame, frame_len);
    }

    if (action == HYBBX_CIRCUIT_BAL_CANCEL) {
        if (circuit_bandwidth_spare_link(hub, slot, action)) {
            return;
        }
        fprintf(stderr,
                "[circuit] load-balance cancelled link %s (%s)\n",
                slot->link_id[0] != '\0' ? slot->link_id : "?",
                reason_str);
    } else if (action == HYBBX_CIRCUIT_BAL_PAUSE ||
               action == HYBBX_CIRCUIT_BAL_BREAK ||
               action == HYBBX_CIRCUIT_BAL_RESUME) {
        printf("[circuit] load-balance %s link=%s (%s)\n",
               hybbx_circuit_balance_action_name(action),
               slot->link_id[0] != '\0' ? slot->link_id : "?",
               reason_str);
    }

    if (hub->service != NULL &&
        (action == HYBBX_CIRCUIT_BAL_PAUSE ||
         action == HYBBX_CIRCUIT_BAL_BREAK ||
         action == HYBBX_CIRCUIT_BAL_RESUME)) {
        (void)hybbx_bandwidth_policy_apply(hub->service, action);
    }
}

static void circuit_balance_tick_slot(hybbx_circuit_link_slot_t *slot,
                                      int *cancel_link)
{
    balance_send_ctx_t bctx;
    flow_ctrl_ctx_t fctx;
    hybbx_circuit_balance_tick_result_t tr;
    hybbx_circuit_hub_t *hub;

    if (cancel_link != NULL) {
        *cancel_link = 0;
    }

    if (slot == NULL || slot->hub == NULL || slot->balance == NULL ||
        !slot->profile_set) {
        return;
    }

    hub = slot->hub;
    bctx.slot = slot;
    fctx.hub = hub;
    fctx.slot = slot;

    tr = hybbx_circuit_balance_tick(slot->balance, HYBBX_CIRCUIT_LINK_POLL_MS,
                                  balance_send_raw_cb, &bctx,
                                  balance_flow_ctrl_cb, &fctx);
    if (slot->balance != NULL && slot->profile_set &&
        hub->config.balance.enabled &&
        slot->profile.bandwidth == HYBBX_CIRCUIT_BW_LOW &&
        hybbx_circuit_balance_action(slot->balance) == HYBBX_CIRCUIT_BAL_PAUSE &&
        hybbx_circuit_balance_queued_bytes(slot->balance) >=
        hub->config.balance.queue_pause) {
        (void)circuit_bandwidth_spare_link(hub, slot, HYBBX_CIRCUIT_BAL_PAUSE);
    }
    if (tr == HYBBX_CIRCUIT_BAL_TICK_CANCEL_LINK && cancel_link != NULL) {
        if (circuit_bandwidth_spare_link(hub, slot, HYBBX_CIRCUIT_BAL_CANCEL)) {
            tr = HYBBX_CIRCUIT_BAL_TICK_OK;
        } else {
            *cancel_link = 1;
        }
    }
}

static hybbx_result_t circuit_slot_send_hbx(hybbx_circuit_link_slot_t *slot,
                                            const uint8_t *frame, size_t len)
{
    balance_send_ctx_t bctx;
    flow_ctrl_ctx_t fctx;
    hybbx_circuit_hub_t *hub;

    if (slot == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    hub = slot->hub;
    if (hub == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (slot->balance != NULL && slot->profile_set &&
        hub->config.balance.enabled) {
        bctx.slot = slot;
        fctx.hub = hub;
        fctx.slot = slot;
        return hybbx_circuit_balance_submit(slot->balance, frame, len,
                                            balance_send_raw_cb, &bctx,
                                            balance_flow_ctrl_cb, &fctx);
    }

    return circuit_slot_send_raw(slot, frame, len);
}

hybbx_result_t hybbx_circuit_hub_send_raw(hybbx_circuit_hub_t *hub,
                                          const uint8_t *frame, size_t len)
{
    unsigned i;

    if (hub == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state != CIRCUIT_SLOT_FREE && hub->slots[i].fd >= 0) {
            return circuit_slot_send_raw(&hub->slots[i], frame, len);
        }
    }

    return HYBBX_ERR_BUSY;
}

static hybbx_result_t circuit_transport_write(hybbx_session_t *session,
                                              const char *data, size_t len)
{
    hybbx_circuit_link_slot_t *slot;
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;

    if (session == NULL || data == NULL || len == 0) {
        return HYBBX_OK;
    }

    slot = (hybbx_circuit_link_slot_t *)session->transport_data;
    if (slot == NULL) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_circuit_encode_terminal(data, len, frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return circuit_slot_send_hbx(slot, frame, frame_len);
}

hybbx_result_t hybbx_circuit_hub_send_hbx(hybbx_circuit_hub_t *hub,
                                          const uint8_t *frame, size_t len)
{
    if (hub == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_circuit_hub_multicast_hbx(hub, frame, len, 0.0, 0);
}

unsigned hybbx_circuit_hub_active_link_count(const hybbx_circuit_hub_t *hub)
{
    unsigned i;
    unsigned count = 0;

    if (hub == NULL) {
        return 0;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state == CIRCUIT_SLOT_ACTIVE) {
            count++;
        }
    }

    return count;
}

hybbx_result_t hybbx_circuit_hub_multicast_hbx(hybbx_circuit_hub_t *hub,
                                               const uint8_t *frame, size_t len,
                                               double frequency_mhz,
                                               int require_broadcast_qos)
{
    unsigned i;
    int sent = 0;
    hybbx_result_t last_err = HYBBX_ERR_NOT_FOUND;

    if (hub == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        hybbx_circuit_link_slot_t *slot = &hub->slots[i];
        hybbx_result_t rc;

        if (slot->state != CIRCUIT_SLOT_ACTIVE || slot->fd < 0) {
            continue;
        }
        if (require_broadcast_qos && !circuit_slot_broadcast_qos(slot)) {
            continue;
        }
        if (frequency_mhz > 0.0 && slot->profile.frequency_mhz > 0.0 &&
            !hybbx_ax25_frequency_match(frequency_mhz,
                                         slot->profile.frequency_mhz)) {
            continue;
        }
        if (require_broadcast_qos && circuit_frame_is_ax25_ui_tx(frame, len) &&
            !circuit_slot_can_send_low_prio(hub, slot)) {
            last_err = HYBBX_ERR_BUSY;
            continue;
        }

        if (require_broadcast_qos && circuit_frame_is_ax25_ui_tx(frame, len)) {
            /*
             * Low-priority AX.25 broadcast: admission is decided by balancer
             * state/queue reserve, then transmit immediately.
             * This avoids enqueue->break drops that can otherwise log as sent
             * without reaching RF.
             */
            rc = circuit_slot_send_raw(slot, frame, len);
        } else {
            rc = circuit_slot_send_hbx(slot, frame, len);
        }
        if (rc == HYBBX_OK) {
            sent++;
        } else {
            last_err = rc;
        }
    }

    if (sent > 0) {
        return HYBBX_OK;
    }

    if (require_broadcast_qos) {
        if (last_err != HYBBX_ERR_NOT_FOUND) {
            return last_err;
        }
        return HYBBX_ERR_DENIED;
    }

    return last_err;
}

double hybbx_circuit_hub_link_frequency_mhz(const hybbx_circuit_hub_t *hub)
{
    unsigned i;

    if (hub == NULL) {
        return 0.0;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state == CIRCUIT_SLOT_ACTIVE && hub->slots[i].profile_set) {
            return hub->slots[i].profile.frequency_mhz;
        }
    }

    return 0.0;
}

int hybbx_circuit_hub_link_broadcast_qos(const hybbx_circuit_hub_t *hub)
{
    unsigned i;

    if (hub == NULL) {
        return 0;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state == CIRCUIT_SLOT_ACTIVE &&
            circuit_slot_broadcast_qos(&hub->slots[i])) {
            return 1;
        }
    }

    return 0;
}

static void circuit_broadcast_links_sort(hybbx_circuit_broadcast_link_t *links,
                                         unsigned count)
{
    unsigned i;
    unsigned j;

    for (i = 1; i < count; i++) {
        hybbx_circuit_broadcast_link_t key = links[i];

        j = i;
        while (j > 0) {
            double prev_mhz = links[j - 1].frequency_mhz;
            double key_mhz = key.frequency_mhz;
            int move = 0;

            if (key_mhz > 0.0 && prev_mhz <= 0.0) {
                move = 0;
            } else if (key_mhz <= 0.0 && prev_mhz > 0.0) {
                move = 1;
            } else if (key_mhz > 0.0 && prev_mhz > 0.0 &&
                       key_mhz < prev_mhz) {
                move = 1;
            } else if (key_mhz <= 0.0 && prev_mhz <= 0.0 &&
                       strcmp(key.link_id, links[j - 1].link_id) < 0) {
                move = 1;
            }

            if (!move) {
                break;
            }

            links[j] = links[j - 1];
            j--;
        }
        links[j] = key;
    }
}

unsigned hybbx_circuit_hub_broadcast_links(const hybbx_circuit_hub_t *hub,
                                           hybbx_circuit_broadcast_link_t *out,
                                           unsigned out_max)
{
    unsigned i;
    unsigned count = 0;

    if (hub == NULL || out == NULL || out_max == 0) {
        return 0;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        const hybbx_circuit_link_slot_t *slot = &hub->slots[i];

        if (slot->state != CIRCUIT_SLOT_ACTIVE || slot->fd < 0) {
            continue;
        }
        if (!circuit_slot_broadcast_qos(slot)) {
            continue;
        }
        if (count >= out_max) {
            break;
        }

        out[count].frequency_mhz = slot->profile.frequency_mhz;
        out[count].slot_index = i;
        hybbx_strlcpy(out[count].link_id, slot->link_id,
                      sizeof(out[count].link_id));
        count++;
    }

    if (count > 1) {
        circuit_broadcast_links_sort(out, count);
    }

    return count;
}

static void on_circuit_frame(hybbx_circuit_proto_t proto, uint16_t flags,
                             const uint8_t *payload, size_t len,
                             void *userdata)
{
    hybbx_circuit_link_slot_t *slot = (hybbx_circuit_link_slot_t *)userdata;
    uint8_t ui[HYBBX_AX25_PAYLOAD_MAX];
    hybbx_ax25_path_t path;

    (void)flags;

    if (slot == NULL) {
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_PROXY_MAIL ||
        proto == HYBBX_CIRCUIT_PROTO_PROXY_CHAT) {
        if (slot->hub != NULL && slot->hub->service != NULL) {
            hybbx_mains_proxy_inbound_frame(slot->hub->service, proto,
                                            payload, len);
        }
        return;
    }

    if (slot->session == NULL || len == 0) {
        return;
    }

    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_AX25: {
        size_t ui_len = hybbx_ax25_parse_ui(payload, len, &path, ui,
                                            sizeof(ui));
        if (ui_len > 0) {
            (void)hybbx_session_handle_input(slot->session, ui, ui_len);
        }
        break;
    }
    case HYBBX_CIRCUIT_PROTO_AX25_UI: {
        size_t ui_len = hybbx_circuit_unpack_ax25_ui(payload, len, &path,
                                                      ui, sizeof(ui));
        if (ui_len > 0) {
            (void)hybbx_session_handle_input(slot->session, ui, ui_len);
        }
        break;
    }
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        (void)hybbx_session_handle_input(slot->session, payload, len);
        break;
    default:
        printf("[circuit] link=%s ignored proto=%s (%u bytes)\n",
               slot->link_id[0] != '\0' ? slot->link_id : "?",
               hybbx_circuit_proto_name(proto), (unsigned)len);
        break;
    }
}

static void circuit_slot_reset(hybbx_circuit_link_slot_t *slot)
{
    if (slot == NULL) {
        return;
    }

    if (slot->balance != NULL) {
        hybbx_circuit_balance_destroy(slot->balance);
        slot->balance = NULL;
    }

    if (slot->session != NULL) {
        hybbx_session_close(slot->session);
        slot->session = NULL;
    }

    slot->profile_set = 0;
    memset(&slot->profile, 0, sizeof(slot->profile));
    slot->link_id[0] = '\0';
    slot->state = CIRCUIT_SLOT_FREE;
    slot->thread = (pthread_t)0;
}

static void circuit_close_slot(hybbx_circuit_link_slot_t *slot)
{
    hybbx_circuit_hub_t *hub;
    int fd;

    if (slot == NULL || slot->hub == NULL) {
        return;
    }

    hub = slot->hub;

    pthread_mutex_lock(&hub->lock);
    fd = slot->fd;
    if (fd >= 0) {
        close(fd);
        slot->fd = -1;
    }
    pthread_mutex_unlock(&hub->lock);

    circuit_slot_reset(slot);
}

static void circuit_close_all_slots(hybbx_circuit_hub_t *hub)
{
    unsigned i;

    if (hub == NULL) {
        return;
    }

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        if (hub->slots[i].state != CIRCUIT_SLOT_FREE || hub->slots[i].fd >= 0) {
            circuit_close_slot(&hub->slots[i]);
        }
    }
}

typedef struct circuit_auth_ctx {
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_link_slot_t *slot;
    int done;
    int ok;
    hybbx_link_auth_t auth;
} circuit_auth_ctx_t;

static void circuit_log_auth_fail(int fd, const char *reason, const char *id)
{
    char ip[HYBBX_REMOTE_ADDR_MAX];

    if (reason == NULL) {
        return;
    }

    if (fd >= 0 && hybbx_socket_peer_name(fd, ip, sizeof(ip)) == HYBBX_OK) {
        if (id != NULL && id[0] != '\0') {
            hybbx_security_log_write(
                "link_auth_fail ip=%s id=%s reason=%s transport=circuit",
                ip, id, reason);
        } else {
            hybbx_security_log_write(
                "link_auth_fail ip=%s reason=%s transport=circuit",
                ip, reason);
        }
        hybbx_security_ban_link_auth_fail(ip);
        if (id != NULL && id[0] != '\0' && strcmp(reason, "banned") != 0) {
            hybbx_security_ban_link_auth_fail_callid(id);
        }
    } else {
        hybbx_security_log_write(
            "link_auth_fail ip=? reason=%s transport=circuit", reason);
    }
}

static int circuit_auth_password_ok(const hybbx_circuit_hub_t *hub,
                                  const hybbx_circuit_bridge_entry_t *entry,
                                  const char *password)
{
    if (hub == NULL || password == NULL) {
        return 0;
    }

    if (entry != NULL && entry->link_password[0] != '\0') {
        return hybbx_password_match(entry->link_password, password);
    }

    if (hub->link_password[0] != '\0') {
        return hybbx_password_match(hub->link_password, password);
    }

    return 0;
}

static int circuit_validate_link_auth(circuit_auth_ctx_t *ctx, const char **reason)
{
    const hybbx_circuit_bridge_entry_t *entry = NULL;
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_link_slot_t *slot;

    if (ctx == NULL || ctx->hub == NULL || ctx->slot == NULL) {
        if (reason != NULL) {
            *reason = "invalid";
        }
        return 0;
    }

    hub = ctx->hub;
    slot = ctx->slot;

    if (!hub->link_auth) {
        return 1;
    }

    if (hub->bridge.count > 0) {
        entry = hybbx_circuit_bridge_find(&hub->bridge, ctx->auth.id);
        if (entry == NULL) {
            if (reason != NULL) {
                *reason = "unknown_id";
            }
            return 0;
        }
    } else if (hub->link_password[0] == '\0') {
        if (reason != NULL) {
            *reason = "no_password";
        }
        return 0;
    }

    if (!circuit_auth_password_ok(hub, entry, ctx->auth.password)) {
        if (reason != NULL) {
            *reason = "password";
        }
        return 0;
    }

    if (circuit_find_active_slot_by_id(hub, ctx->auth.id, slot) >= 0) {
        if (reason != NULL) {
            *reason = "duplicate_id";
        }
        return 0;
    }

    (void)entry;
    (void)slot;
    return 1;
}

static void circuit_on_auth_frame(hybbx_circuit_proto_t proto, uint16_t flags,
                                  const uint8_t *payload, size_t len,
                                  void *userdata)
{
    circuit_auth_ctx_t *ctx = (circuit_auth_ctx_t *)userdata;
    const hybbx_circuit_bridge_entry_t *entry;
    char code[HYBBX_LINK_CODE_MAX];
    char ack[HYBBX_LINK_AUTH_PAYLOAD_MAX];
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    const char *fail_reason = "invalid";
    int fd;

    (void)flags;

    if (ctx == NULL || ctx->done || ctx->slot == NULL) {
        return;
    }

    if (proto != HYBBX_CIRCUIT_PROTO_LINK_AUTH) {
        return;
    }

    fd = ctx->slot->fd;

    if (hybbx_link_auth_parse((const char *)payload, len, &ctx->auth) != HYBBX_OK) {
        circuit_log_auth_fail(fd, "invalid", NULL);
        ctx->done = 1;
        ctx->ok = 0;
        return;
    }

    if (!hybbx_security_ban_callid_accept(ctx->auth.id)) {
        circuit_log_auth_fail(fd, "banned", ctx->auth.id);
        ctx->done = 1;
        ctx->ok = 0;
        return;
    }

    if (!circuit_validate_link_auth(ctx, &fail_reason)) {
        fprintf(stderr, "[circuit] link auth failed for id=%s (%s)\n",
                ctx->auth.id, fail_reason);
        circuit_log_auth_fail(fd, fail_reason, ctx->auth.id);
        ctx->done = 1;
        ctx->ok = 0;
        return;
    }

    code[0] = '\0';
    (void)hybbx_link_registry_touch(&ctx->hub->links, ctx->auth.id,
                                    ctx->auth.role, code, sizeof(code));

    snprintf(ack, sizeof(ack), "ok=yes\nid=%s\ncode=%s\n",
             ctx->auth.id, code[0] != '\0' ? code : "-");
    frame_len = hybbx_circuit_encode_link_msg(HYBBX_CIRCUIT_PROTO_LINK_AUTH_ACK,
                                              ack, strlen(ack),
                                              frame, sizeof(frame));
    if (frame_len > 0) {
        (void)circuit_slot_send_raw(ctx->slot, frame, frame_len);
    }

    hybbx_strlcpy(ctx->slot->link_id, ctx->auth.id, sizeof(ctx->slot->link_id));

    hybbx_circuit_link_profile_from_auth(&ctx->auth, &ctx->slot->profile);
    entry = hybbx_circuit_bridge_find(&ctx->hub->bridge, ctx->auth.id);
    if (entry != NULL && entry->frequency_mhz > 0.0 &&
        ctx->slot->profile.frequency_mhz <= 0.0) {
        ctx->slot->profile.frequency_mhz = entry->frequency_mhz;
    }

    if (ctx->slot->balance != NULL) {
        hybbx_circuit_balance_set_profile(ctx->slot->balance, &ctx->slot->profile);
    }
    ctx->slot->profile_set = 1;

    printf("[circuit] link authenticated id=%s role=%s code=%s\n",
           ctx->auth.id, ctx->auth.role, code[0] != '\0' ? code : "-");
    printf("[circuit] link QoS bandwidth=%s baud=%u duplex=%s",
           ctx->slot->profile.bandwidth == HYBBX_CIRCUIT_BW_LOW ?
           "low" : "high",
           ctx->slot->profile.baud,
           ctx->slot->profile.duplex == HYBBX_CIRCUIT_DUPLEX_HALF ?
           "half" : "full");
    if (ctx->slot->profile.frequency_mhz > 0.0) {
        printf(" %.3fMHz", ctx->slot->profile.frequency_mhz);
    }
    printf("\n");

    ctx->done = 1;
    ctx->ok = 1;
}

static int circuit_wait_link_auth(hybbx_circuit_link_slot_t *slot)
{
    circuit_auth_ctx_t ctx;
    hybbx_circuit_decoder_t dec;
    uint8_t buf[256];
    unsigned elapsed = 0;
    hybbx_circuit_hub_t *hub;

    if (slot == NULL || slot->hub == NULL) {
        return 0;
    }

    hub = slot->hub;

    if (!hub->link_auth) {
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.hub = hub;
    ctx.slot = slot;
    hybbx_circuit_decoder_init(&dec);

    while (!ctx.done && elapsed < HYBBX_CIRCUIT_AUTH_TIMEOUT_MS) {
        struct pollfd pfd;
        ssize_t n;
        int pr;

        pthread_mutex_lock(&hub->lock);
        pfd.fd = slot->fd;
        pthread_mutex_unlock(&hub->lock);

        if (pfd.fd < 0) {
            return 0;
        }

        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (pr == 0) {
            elapsed += HYBBX_CIRCUIT_LINK_POLL_MS;
            continue;
        }
        if ((pfd.revents & POLLIN) == 0) {
            return 0;
        }

        n = recv(pfd.fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            return 0;
        }

        hybbx_circuit_decoder_feed(&dec, buf, (size_t)n,
                                   circuit_on_auth_frame, &ctx);
    }

    if (!ctx.done) {
        circuit_log_auth_fail(slot->fd, "timeout_no_link_auth", NULL);
    }

    return ctx.ok;
}

static void *circuit_link_thread(void *arg)
{
    hybbx_circuit_link_slot_t *slot = (hybbx_circuit_link_slot_t *)arg;
    hybbx_circuit_hub_t *hub;
    uint8_t buf[512];
    hybbx_result_t rc;

    if (slot == NULL || slot->hub == NULL) {
        return NULL;
    }

    hub = slot->hub;

    if (!circuit_wait_link_auth(slot)) {
        fprintf(stderr, "[circuit] link authentication failed or timed out\n");
        circuit_close_slot(slot);
        return NULL;
    }

    if (!slot->profile_set) {
        hybbx_circuit_link_profile_from_auth(NULL, &slot->profile);
        if (slot->balance != NULL) {
            hybbx_circuit_balance_set_profile(slot->balance, &slot->profile);
        }
        slot->profile_set = 1;
    }

    hybbx_circuit_decoder_init(&slot->decoder);

    rc = hybbx_session_open(hub->service, &hybbx_plugin_circuit, slot,
                            &slot->session);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[circuit] session open failed for link %s\n",
                slot->link_id[0] != '\0' ? slot->link_id : "?");
        circuit_close_slot(slot);
        return NULL;
    }

    {
        char remote[HYBBX_REMOTE_ADDR_MAX];
        int fd = slot->fd;

        if (fd >= 0 &&
            hybbx_socket_peer_name(fd, remote, sizeof(remote)) == HYBBX_OK) {
            (void)hybbx_session_set_remote(slot->session, remote);
        }
    }

    slot->state = CIRCUIT_SLOT_ACTIVE;
    printf("[circuit] link adapter attached id=%s (HBX bridge active)\n",
           slot->link_id[0] != '\0' ? slot->link_id : "?");

    while (hub->running) {
        struct pollfd pfd;
        ssize_t n;
        int pr;

        pthread_mutex_lock(&hub->lock);
        pfd.fd = slot->fd;
        pthread_mutex_unlock(&hub->lock);

        if (pfd.fd < 0) {
            break;
        }

        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            int cancel_link = 0;

            circuit_balance_tick_slot(slot, &cancel_link);
            if (cancel_link) {
                break;
            }
            if (slot->session != NULL) {
                rc = hybbx_session_tick(slot->session);
                if (rc == HYBBX_SESSION_END) {
                    break;
                }
            }
            continue;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }
        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }

        n = recv(pfd.fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }

        hybbx_circuit_decoder_feed(&slot->decoder, buf, (size_t)n,
                                   on_circuit_frame, slot);

        {
            int cancel_link = 0;
            circuit_balance_tick_slot(slot, &cancel_link);
            if (cancel_link) {
                break;
            }
        }
    }

    printf("[circuit] link detached id=%s\n",
           slot->link_id[0] != '\0' ? slot->link_id : "?");
    circuit_close_slot(slot);
    return NULL;
}

static void *circuit_accept_thread(void *arg)
{
    hybbx_circuit_hub_t *hub = (hybbx_circuit_hub_t *)arg;

    while (hub->running) {
        struct pollfd pfds[2];
        int count = 0;
        int pr;
        int i;

        pfds[0].fd = hub->listen_v4;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        if (hub->listen_v4 >= 0) {
            count = 1;
        }

        pfds[1].fd = hub->listen_v6;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;
        if (hub->listen_v6 >= 0) {
            count = 2;
        }

        if (count == 0) {
            break;
        }

        pr = poll(pfds, (nfds_t)count, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            continue;
        }

        for (i = 0; i < count; i++) {
            if ((pfds[i].revents & POLLIN) == 0) {
                continue;
            }

            {
                int client = accept(pfds[i].fd, NULL, NULL);
                int slot_idx;
                hybbx_circuit_link_slot_t *slot;

                if (client < 0) {
                    continue;
                }

                (void)set_socket_options(client, 0);

                if (!hybbx_security_ban_accept_fd(client)) {
                    close(client);
                    continue;
                }

                pthread_mutex_lock(&hub->lock);
                if (circuit_count_used_slots(hub) >= hub->max_links) {
                    pthread_mutex_unlock(&hub->lock);
                    fprintf(stderr,
                            "[circuit] max_links=%u reached — rejecting connection\n",
                            hub->max_links);
                    close(client);
                    continue;
                }

                slot_idx = circuit_find_free_slot(hub);
                if (slot_idx < 0) {
                    pthread_mutex_unlock(&hub->lock);
                    close(client);
                    continue;
                }

                slot = &hub->slots[slot_idx];
                slot->fd = client;
                slot->state = CIRCUIT_SLOT_CONNECTING;
                slot->balance = hybbx_circuit_balance_create(&hub->config.balance);
                pthread_mutex_unlock(&hub->lock);

                if (pthread_create(&slot->thread, NULL, circuit_link_thread,
                                   slot) != 0) {
                    fprintf(stderr, "[circuit] link thread failed\n");
                    circuit_close_slot(slot);
                } else {
                    pthread_detach(slot->thread);
                }
            }
        }
    }

    return NULL;
}

hybbx_circuit_hub_t *hybbx_circuit_hub_create(hybbx_service_t *service)
{
    hybbx_circuit_hub_t *hub;
    unsigned i;

    hub = calloc(1, sizeof(*hub));
    if (hub == NULL) {
        return NULL;
    }

    hub->service = service;
    hub->listen_v4 = -1;
    hub->listen_v6 = -1;
    hub->max_links = HYBBX_CIRCUIT_DEFAULT_MAX_LINKS;
    pthread_mutex_init(&hub->lock, NULL);

    for (i = 0; i < HYBBX_CIRCUIT_MAX_LINKS; i++) {
        hub->slots[i].hub = hub;
        hub->slots[i].fd = -1;
        hub->slots[i].state = CIRCUIT_SLOT_FREE;
    }

    return hub;
}

void hybbx_circuit_hub_destroy(hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return;
    }

    hybbx_circuit_hub_stop(hub);
    pthread_mutex_destroy(&hub->lock);
    free(hub);
}

hybbx_result_t hybbx_circuit_hub_start(hybbx_circuit_hub_t *hub,
                                       const hybbx_circuit_config_t *cfg)
{
    int backlog;

    if (hub == NULL || cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_circuit_hub_stop(hub);
    hub->config = *cfg;
    hub->bridge = cfg->bridge;
    hub->max_links = cfg->max_links;
    if (hub->max_links == 0 || hub->max_links > HYBBX_CIRCUIT_MAX_LINKS) {
        hub->max_links = HYBBX_CIRCUIT_DEFAULT_MAX_LINKS;
    }
    hybbx_strlcpy(hub->link_password, cfg->link_password, sizeof(hub->link_password));
    hub->link_auth = cfg->link_auth;
    hybbx_link_registry_init(&hub->links, cfg->data_path, cfg->config_path,
                             cfg->link_stale_days);
    (void)hybbx_link_registry_prune(&hub->links);
    hub->running = 1;

    backlog = (int)hub->max_links;
    if (backlog < 4) {
        backlog = 4;
    }

    if (cfg->ipv4) {
        hub->listen_v4 = create_listen_socket(AF_INET, cfg->bind4, cfg->port,
                                              backlog);
        if (hub->listen_v4 < 0) {
            hybbx_socket_log_bind_failure("circuit", cfg->bind4, cfg->port);
            hybbx_circuit_hub_stop(hub);
            return HYBBX_ERR_IO;
        }
    }

    if (cfg->ipv6) {
        hub->listen_v6 = create_listen_socket(AF_INET6, cfg->bind6, cfg->port,
                                              backlog);
        if (hub->listen_v6 < 0) {
            fprintf(stderr, "[circuit] IPv6 bind [%s]:%u skipped (%s)\n",
                    cfg->bind6, cfg->port, strerror(errno));
        }
    }

    if (hub->listen_v4 < 0 && hub->listen_v6 < 0) {
        hybbx_circuit_hub_stop(hub);
        return HYBBX_ERR_IO;
    }

    printf("[circuit] internal TCP hub");
    if (hub->listen_v4 >= 0) {
        printf(" %s:%u", cfg->bind4, cfg->port);
    }
    if (hub->listen_v6 >= 0) {
        printf(" [%s]:%u", cfg->bind6, cfg->port);
    }
    printf(" (HBX v%u, max_links=%u, bridge=%u)\n",
           (unsigned)HYBBX_CIRCUIT_VERSION, hub->max_links, hub->bridge.count);

    if (pthread_create(&hub->accept_thread, NULL, circuit_accept_thread,
                       hub) != 0) {
        hybbx_circuit_hub_stop(hub);
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

void hybbx_circuit_hub_stop(hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return;
    }

    hub->running = 0;

    if (hub->listen_v4 >= 0) {
        close(hub->listen_v4);
        hub->listen_v4 = -1;
    }
    if (hub->listen_v6 >= 0) {
        close(hub->listen_v6);
        hub->listen_v6 = -1;
    }

    circuit_close_all_slots(hub);

    if (hub->accept_thread) {
        pthread_join(hub->accept_thread, NULL);
        hub->accept_thread = (pthread_t)0;
    }
}

int hybbx_circuit_hub_running(const hybbx_circuit_hub_t *hub)
{
    return hub != NULL && hub->running;
}

unsigned hybbx_circuit_hub_port(const hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return HYBBX_CIRCUIT_DEFAULT_PORT;
    }

    return hub->config.port;
}

void hybbx_circuit_hub_prune_links(hybbx_circuit_hub_t *hub)
{
    if (hub != NULL) {
        (void)hybbx_link_registry_prune(&hub->links);
    }
}

static hybbx_result_t circuit_plugin_init(hybbx_service_t *service)
{
    (void)service;
    return HYBBX_OK;
}

static void circuit_plugin_shutdown(void)
{
}

static hybbx_result_t circuit_plugin_start(const char *config)
{
    (void)config;
    return HYBBX_OK;
}

static hybbx_result_t circuit_plugin_stop(void)
{
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_circuit = {
    .name = "circuit",
    .kind = HYBBX_TRANSPORT_CIRCUIT,
    .version = 1,
    .init = circuit_plugin_init,
    .shutdown = circuit_plugin_shutdown,
    .start = circuit_plugin_start,
    .stop = circuit_plugin_stop,
    .write = circuit_transport_write,
};
