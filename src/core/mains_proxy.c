#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/mains_proxy.h"
#include "hybbx/service.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/proxymail.h"
#include "hybbx/proxychat.h"
#include "hybbx/log.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Inter-node mesh: hybbx_circuit_link_connect() only — never peer Main TCP. */

#define MAINS_PROXY_RECONNECT_MS 5000u
#define MAINS_PROXY_CONNECT_ATTEMPTS 30u

typedef struct mains_proxy_peer_runtime {
    hybbx_mains_proxy_peer_config_t config;
    int fd;
    hybbx_circuit_decoder_t dec;
    unsigned reconnect_ms;
} mains_proxy_peer_runtime_t;

typedef struct mains_proxy_runtime {
    hybbx_service_t *service;
    hybbx_mains_proxy_mesh_t mesh;
    mains_proxy_peer_runtime_t peers[HYBBX_MAINS_PROXY_MAX_PEERS];
    unsigned live_links;
} mains_proxy_runtime_t;

static mains_proxy_runtime_t g_rt;

static const char *mains_proxy_find_kv(const char *config, const char *key,
                                       char *scratch, size_t scratch_len)
{
    const char *cursor = config;
    size_t key_len = strlen(key);

    if (scratch != NULL && scratch_len > 0) {
        scratch[0] = '\0';
    }

    if (config == NULL || key == NULL) {
        return NULL;
    }

    while (*cursor != '\0') {
        const char *line = cursor;
        const char *eq;
        const char *end = strchr(cursor, '\n');

        if (end == NULL) {
            end = cursor + strlen(cursor);
        }

        if (line[0] == '[') {
            cursor = (*end == '\0') ? end : end + 1;
            continue;
        }

        eq = strchr(line, '=');
        if (eq == NULL || eq >= end) {
            cursor = (*end == '\0') ? end : end + 1;
            continue;
        }

        if ((size_t)(eq - line) == key_len &&
            strncmp(line, key, key_len) == 0) {
            const char *value = eq + 1;

            while (value < end && (*value == ' ' || *value == '\t')) {
                value++;
            }

            if (scratch != NULL && scratch_len > 0) {
                size_t vlen = (size_t)(end - value);

                while (vlen > 0 &&
                       (value[vlen - 1] == ' ' || value[vlen - 1] == '\t' ||
                        value[vlen - 1] == '\r')) {
                    vlen--;
                }

                if (vlen >= scratch_len) {
                    vlen = scratch_len - 1;
                }
                memcpy(scratch, value, vlen);
                scratch[vlen] = '\0';
                return scratch;
            }

            return value;
        }

        cursor = (*end == '\0') ? end : end + 1;
    }

    return NULL;
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

static int proxy_payload_get_line(const char *payload, size_t len,
                                  const char *key, char *out, size_t out_len)
{
    size_t key_len;
    const char *cursor;
    const char *end;

    if (payload == NULL || key == NULL || out == NULL || out_len == 0) {
        return 0;
    }

    out[0] = '\0';
    key_len = strlen(key);
    cursor = payload;
    end = payload + len;

    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        size_t line_len;

        if (line_end == NULL) {
            line_end = end;
        }

        line_len = (size_t)(line_end - cursor);
        if (line_len >= key_len + 1 && memcmp(cursor, key, key_len) == 0 &&
            cursor[key_len] == '=') {
            const char *value = cursor + key_len + 1;
            size_t vlen = line_len - key_len - 1;

            while (vlen > 0 && (*value == ' ' || *value == '\t')) {
                value++;
                vlen--;
            }
            if (vlen >= out_len) {
                vlen = out_len - 1;
            }
            memcpy(out, value, vlen);
            out[vlen] = '\0';
            return 1;
        }

        if (line_end >= end) {
            break;
        }
        cursor = line_end + 1;
    }

    return 0;
}

static const char *proxy_payload_body(const char *payload, size_t len,
                                      size_t *body_len)
{
    size_t off;

    if (payload == NULL || body_len == NULL) {
        return NULL;
    }

    *body_len = 0;

    if (len >= 5 && memcmp(payload, "---\r\n", 5) == 0) {
        off = 5;
    } else if (len >= 4 && memcmp(payload, "---\n", 4) == 0) {
        off = 4;
    } else {
        return NULL;
    }

    while (off < len && (payload[off] == '\r' || payload[off] == '\n')) {
        off++;
    }

    *body_len = len - off;
    return payload + off;
}

static size_t proxy_build_mail_payload(char *out, size_t out_cap,
                                       const char *from_address,
                                       const char *to_address,
                                       const char *subject,
                                       const char *body)
{
    int n;

    if (out == NULL || out_cap == 0 || from_address == NULL ||
        to_address == NULL || subject == NULL || body == NULL) {
        return 0;
    }

    n = snprintf(out, out_cap, "from=%s\nto=%s\nsub=%s\n---\n%s",
                 from_address, to_address, subject, body);
    if (n < 0 || (size_t)n >= out_cap) {
        return 0;
    }

    return (size_t)n;
}

static size_t proxy_build_chat_payload(char *out, size_t out_cap,
                                       const char *from_address,
                                       const char *line)
{
    int n;

    if (out == NULL || out_cap == 0 || from_address == NULL ||
        line == NULL) {
        return 0;
    }

    n = snprintf(out, out_cap, "from=%s\n---\n%s", from_address, line);
    if (n < 0 || (size_t)n >= out_cap) {
        return 0;
    }

    return (size_t)n;
}

static void proxy_dispatch_payload(hybbx_service_t *service,
                                   hybbx_circuit_proto_t proto,
                                   const uint8_t *payload, size_t len)
{
    char from[HYBBX_PROXYMAIL_ADDRESS_MAX];
    char to[HYBBX_PROXYMAIL_ADDRESS_MAX];
    char subj[HYBBX_MAIL_SUBJECT_MAX + 1];
    const char *body;
    size_t body_len;
    char scratch[HYBBX_CIRCUIT_MAX_PAYLOAD + 1];

    if (service == NULL || payload == NULL || len == 0 ||
        len > HYBBX_CIRCUIT_MAX_PAYLOAD) {
        return;
    }

    memcpy(scratch, payload, len);
    scratch[len] = '\0';

    if (!proxy_payload_get_line(scratch, len, "from", from, sizeof(from))) {
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_PROXY_MAIL) {
        if (!proxy_payload_get_line(scratch, len, "to", to, sizeof(to))) {
            return;
        }
        if (!proxy_payload_get_line(scratch, len, "sub", subj, sizeof(subj))) {
            subj[0] = '\0';
        }
        body = proxy_payload_body(scratch, len, &body_len);
        if (body == NULL) {
            return;
        }
        scratch[0] = '\0';
        if (body_len > 0) {
            if (body_len >= sizeof(scratch)) {
                body_len = sizeof(scratch) - 1;
            }
            memcpy(scratch, body, body_len);
            scratch[body_len] = '\0';
        }
        hybbx_proxymail_receive(service, from, to, subj, scratch);
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_PROXY_CHAT) {
        body = proxy_payload_body(scratch, len, &body_len);
        if (body == NULL) {
            return;
        }
        scratch[0] = '\0';
        if (body_len > 0) {
            if (body_len >= sizeof(scratch)) {
                body_len = sizeof(scratch) - 1;
            }
            memcpy(scratch, body, body_len);
            scratch[body_len] = '\0';
        }
        hybbx_proxychat_receive(service, from, scratch);
    }
}

static hybbx_result_t proxy_peer_send(int fd, hybbx_circuit_proto_t proto,
                                      const uint8_t *payload, size_t len)
{
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;

    if (fd < 0 || payload == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_circuit_encode(proto, HYBBX_CIRCUIT_FLAG_NONE,
                                     payload, len, frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(fd, frame, frame_len);
}

static void proxy_peer_disconnect(mains_proxy_peer_runtime_t *peer)
{
    if (peer == NULL) {
        return;
    }

    if (peer->fd >= 0) {
        close(peer->fd);
        peer->fd = -1;
    }

    if (g_rt.live_links > 0) {
        g_rt.live_links--;
    }

    peer->reconnect_ms = MAINS_PROXY_RECONNECT_MS;
}

static void on_mesh_frame(hybbx_circuit_proto_t proto, uint16_t flags,
                          const uint8_t *payload, size_t len,
                          void *userdata)
{
    (void)flags;
    (void)userdata;

    if (g_rt.service == NULL) {
        return;
    }

    if (proto != HYBBX_CIRCUIT_PROTO_PROXY_MAIL &&
        proto != HYBBX_CIRCUIT_PROTO_PROXY_CHAT) {
        return;
    }

    proxy_dispatch_payload(g_rt.service, proto, payload, len);
}

static int peer_name_matches(const hybbx_mains_proxy_peer_config_t *peer,
                             const char *remote_service)
{
    if (peer == NULL || remote_service == NULL || remote_service[0] == '\0') {
        return 0;
    }

    if (peer->peer_id[0] != '\0' &&
        str_ieq_local(peer->peer_id, remote_service)) {
        return 1;
    }

    if (peer->link_id[0] != '\0' &&
        str_ieq_local(peer->link_id, remote_service)) {
        return 1;
    }

    return 0;
}

static mains_proxy_peer_runtime_t *proxy_find_peer_for_service(
    const char *remote_service)
{
    unsigned i;

    for (i = 0; i < g_rt.mesh.peer_count; i++) {
        if (g_rt.peers[i].fd < 0) {
            continue;
        }
        if (peer_name_matches(&g_rt.peers[i].config, remote_service)) {
            return &g_rt.peers[i];
        }
    }

    return NULL;
}

static hybbx_result_t proxy_peer_connect(mains_proxy_peer_runtime_t *peer)
{
    const hybbx_mains_proxy_peer_config_t *cfg;
    const char *host;
    unsigned port;
    const char *link_id;
    const char *peer_label;
    unsigned attempt;
    hybbx_result_t rc;

    if (peer == NULL) {
        return HYBBX_ERR_INVALID;
    }

    cfg = &peer->config;
    peer_label = cfg->peer_id[0] != '\0' ? cfg->peer_id : "(unnamed)";

    if (!cfg->enabled) {
        return HYBBX_OK;
    }

    if (cfg->wire == HYBBX_MAINS_PROXY_WIRE_AX25) {
        hybbx_log_warn("[mains_proxy] peer '%s': wire=ax25 not active — use circuit",
                peer_label);
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (cfg->circuit_host[0] == '\0') {
        return HYBBX_OK;
    }

    if (cfg->link_id[0] == '\0' || cfg->link_password[0] == '\0') {
        hybbx_log_warn("[mains_proxy] peer '%s' missing link_id or link_password",
                peer_label);
        return HYBBX_ERR_INVALID;
    }

    host = cfg->circuit_host;
    port = cfg->circuit_port;
    if (port == 0) {
        port = HYBBX_CIRCUIT_DEFAULT_PORT;
    }

    link_id = cfg->link_id;

    for (attempt = 0; attempt < MAINS_PROXY_CONNECT_ATTEMPTS; attempt++) {
        rc = hybbx_circuit_link_connect(host, port, &peer->fd);
        if (rc != HYBBX_OK) {
            usleep(100000);
            continue;
        }

        hybbx_circuit_decoder_init(&peer->dec);
        rc = hybbx_circuit_link_authenticate(peer->fd, cfg->link_password,
                                             "proxy", link_id);
        if (rc != HYBBX_OK) {
            close(peer->fd);
            peer->fd = -1;
            usleep(100000);
            continue;
        }

        g_rt.live_links++;
        peer->reconnect_ms = 0;
        hybbx_log_info("[mains_proxy] linked to peer %s via HBX %s:%u link_id=%s",
               peer_label, host, port, link_id);
        return HYBBX_OK;
    }

    hybbx_log_warn("[mains_proxy] could not link peer '%s' at %s:%u",
            peer_label, host, port);
    return HYBBX_ERR_IO;
}

hybbx_mains_proxy_wire_t hybbx_mains_proxy_wire_parse(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_MAINS_PROXY_WIRE_CIRCUIT;
    }

    if (str_ieq_local(value, "circuit") || str_ieq_local(value, "tcp") ||
        str_ieq_local(value, "tcpip") || str_ieq_local(value, "tcp/ip")) {
        return HYBBX_MAINS_PROXY_WIRE_CIRCUIT;
    }

    if (str_ieq_local(value, "ax25") || str_ieq_local(value, "packet_radio")) {
        return HYBBX_MAINS_PROXY_WIRE_AX25;
    }

    return HYBBX_MAINS_PROXY_WIRE_CIRCUIT;
}

const char *hybbx_mains_proxy_wire_name(hybbx_mains_proxy_wire_t wire)
{
    switch (wire) {
    case HYBBX_MAINS_PROXY_WIRE_AX25:
        return "ax25";
    case HYBBX_MAINS_PROXY_WIRE_CIRCUIT:
    default:
        return "circuit";
    }
}

hybbx_mains_proxy_duplex_t hybbx_mains_proxy_duplex_parse(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_MAINS_PROXY_DUPLEX_FULL;
    }

    if (str_ieq_local(value, "half") || str_ieq_local(value, "half-duplex") ||
        str_ieq_local(value, "half_duplex")) {
        return HYBBX_MAINS_PROXY_DUPLEX_HALF;
    }

    return HYBBX_MAINS_PROXY_DUPLEX_FULL;
}

const char *hybbx_mains_proxy_duplex_name(hybbx_mains_proxy_duplex_t duplex)
{
    return duplex == HYBBX_MAINS_PROXY_DUPLEX_HALF ? "half" : "full";
}

void hybbx_mains_proxy_peer_defaults(hybbx_mains_proxy_peer_config_t *peer)
{
    if (peer == NULL) {
        return;
    }

    memset(peer, 0, sizeof(*peer));
    peer->circuit_port = HYBBX_CIRCUIT_DEFAULT_PORT;
    peer->wire = HYBBX_MAINS_PROXY_WIRE_CIRCUIT;
    peer->duplex = HYBBX_MAINS_PROXY_DUPLEX_FULL;
    peer->use_secondary = 1;
    peer->enabled = 1;
}

hybbx_result_t hybbx_mains_proxy_peer_parse(const char *config,
                                            hybbx_mains_proxy_peer_config_t *out)
{
    char scratch[HYBBX_CONFIG_LINE_MAX];
    const char *value;
    int legacy_host = 0;
    int legacy_port = 0;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_mains_proxy_peer_defaults(out);

    if (config == NULL || config[0] == '\0') {
        return HYBBX_OK;
    }

    value = mains_proxy_find_kv(config, "enabled", scratch, sizeof(scratch));
    if (value != NULL) {
        out->enabled = hybbx_parse_bool(value, 1);
    }

    value = mains_proxy_find_kv(config, "peer_id", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->peer_id, value, sizeof(out->peer_id));
    }

    value = mains_proxy_find_kv(config, "circuit_host", scratch,
                                sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->circuit_host, value, sizeof(out->circuit_host));
    }

    value = mains_proxy_find_kv(config, "circuit_port", scratch,
                                sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        unsigned long port = strtoul(value, NULL, 10);

        if (port >= 1u && port <= 65535u) {
            out->circuit_port = (unsigned)port;
        }
    }

    value = mains_proxy_find_kv(config, "link_id", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->link_id, value, sizeof(out->link_id));
    }

    value = mains_proxy_find_kv(config, "link_password", scratch,
                                sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->link_password, value, sizeof(out->link_password));
    }

    value = mains_proxy_find_kv(config, "host", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->host, value, sizeof(out->host));
        legacy_host = 1;
    }

    value = mains_proxy_find_kv(config, "port", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        unsigned long port = strtoul(value, NULL, 10);

        if (port >= 1u && port <= 65535u) {
            out->port = (unsigned)port;
        }
        legacy_port = 1;
    }

    if (legacy_host && out->circuit_host[0] == '\0') {
        hybbx_strlcpy(out->circuit_host, out->host,
                      sizeof(out->circuit_host));
        hybbx_log_warn("[mains_proxy] peer '%s': deprecated key host= — use "
                       "circuit_host= (mapped for now)",
                out->peer_id[0] != '\0' ? out->peer_id : "(unnamed)");
    } else if (legacy_host) {
        hybbx_log_warn("[mains_proxy] peer '%s': deprecated key host= ignored "
                       "(circuit_host set)",
                out->peer_id[0] != '\0' ? out->peer_id : "(unnamed)");
    }

    if (legacy_port) {
        hybbx_log_warn("[mains_proxy] peer '%s': deprecated key port= ignored — "
                       "use circuit_port= (HBX hub, default %u)",
                out->peer_id[0] != '\0' ? out->peer_id : "(unnamed)",
                (unsigned)HYBBX_CIRCUIT_DEFAULT_PORT);
    }

    value = mains_proxy_find_kv(config, "wire", scratch, sizeof(scratch));
    out->wire = hybbx_mains_proxy_wire_parse(value);

    value = mains_proxy_find_kv(config, "duplex", scratch, sizeof(scratch));
    out->duplex = hybbx_mains_proxy_duplex_parse(value);

    value = mains_proxy_find_kv(config, "use_secondary", scratch,
                                sizeof(scratch));
    if (value != NULL) {
        out->use_secondary = hybbx_parse_bool(value, 1);
    }

    return HYBBX_OK;
}

void hybbx_mains_proxy_mesh_init(hybbx_mains_proxy_mesh_t *mesh)
{
    if (mesh == NULL) {
        return;
    }

    memset(mesh, 0, sizeof(*mesh));
}

hybbx_result_t hybbx_mains_proxy_mesh_start(hybbx_service_t *service,
                                            hybbx_mains_proxy_mesh_t *mesh)
{
    unsigned i;
    unsigned configured = 0;
    unsigned linked = 0;

    if (mesh == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_mains_proxy_mesh_stop(mesh);

    memset(&g_rt, 0, sizeof(g_rt));
    g_rt.service = service;
    g_rt.mesh = *mesh;

    for (i = 0; i < mesh->peer_count; i++) {
        g_rt.peers[i].config = mesh->peers[i];
        g_rt.peers[i].fd = -1;
        hybbx_circuit_decoder_init(&g_rt.peers[i].dec);

        if (!mesh->peers[i].enabled) {
            continue;
        }

        configured++;

        if (mesh->peers[i].circuit_host[0] == '\0') {
            continue;
        }

        if (proxy_peer_connect(&g_rt.peers[i]) == HYBBX_OK &&
            g_rt.peers[i].fd >= 0) {
            linked++;
        }
    }

    if (configured == 0) {
        hybbx_log_warn("[mains_proxy] no active peers configured");
        return HYBBX_ERR_NOT_FOUND;
    }

    mesh->running = 1;
    g_rt.mesh.running = 1;

    hybbx_log_info("[mains_proxy] mesh started (%u peer(s) configured, %u HBX link(s))",
           configured, linked);

    return HYBBX_OK;
}

void hybbx_mains_proxy_mesh_stop(hybbx_mains_proxy_mesh_t *mesh)
{
    unsigned i;

    if (mesh != NULL && mesh->running) {
        hybbx_log_info("[mains_proxy] mesh stopped");
        mesh->running = 0;
    }

    for (i = 0; i < HYBBX_MAINS_PROXY_MAX_PEERS; i++) {
        proxy_peer_disconnect(&g_rt.peers[i]);
    }

    memset(&g_rt, 0, sizeof(g_rt));
}

void hybbx_mains_proxy_mesh_tick(hybbx_service_t *service,
                                 hybbx_mains_proxy_mesh_t *mesh)
{
    unsigned i;
    static unsigned tick_ms;

    (void)service;

    if (mesh == NULL || !mesh->running) {
        return;
    }

    tick_ms += 50;
    if (tick_ms >= 1000) {
        tick_ms = 0;
    }

    for (i = 0; i < mesh->peer_count; i++) {
        mains_proxy_peer_runtime_t *peer = &g_rt.peers[i];
        struct pollfd pfd;
        uint8_t buf[512];
        size_t read_len;
        hybbx_result_t rc;
        int pr;

        if (!peer->config.enabled) {
            continue;
        }

        if (peer->fd < 0) {
            if (peer->config.circuit_host[0] == '\0') {
                continue;
            }
            if (peer->reconnect_ms > 0) {
                if (tick_ms != 0) {
                    continue;
                }
                if (peer->reconnect_ms > 50) {
                    peer->reconnect_ms -= 50;
                    continue;
                }
                peer->reconnect_ms = 0;
            }
            (void)proxy_peer_connect(peer);
            continue;
        }

        pfd.fd = peer->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, 0);
        if (pr <= 0) {
            continue;
        }

        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            hybbx_log_stats("[mains_proxy] peer '%s' disconnected",
                   peer->config.peer_id[0] != '\0' ? peer->config.peer_id
                                                   : "(unnamed)");
            proxy_peer_disconnect(peer);
            continue;
        }

        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }

        rc = hybbx_circuit_link_read(peer->fd, buf, sizeof(buf), &read_len);
        if (rc != HYBBX_OK || read_len == 0) {
            proxy_peer_disconnect(peer);
            continue;
        }

        hybbx_circuit_decoder_feed(&peer->dec, buf, read_len,
                                   on_mesh_frame, NULL);
    }
}

int hybbx_mains_proxy_mesh_active(void)
{
    return g_rt.mesh.running;
}

hybbx_result_t hybbx_mains_proxy_send_mail(hybbx_service_t *service,
                                         const char *from_address,
                                         const char *to_address,
                                         const char *subject,
                                         const char *body)
{
    char user[HYBBX_USER_NAME_MAX];
    char remote[HYBBX_PROXYMAIL_SERVICE_NAME_MAX];
    char payload[HYBBX_CIRCUIT_MAX_PAYLOAD + 1];
    size_t payload_len;
    mains_proxy_peer_runtime_t *peer;

    (void)service;

    if (!g_rt.mesh.running || g_rt.live_links == 0) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (!hybbx_proxymail_parse_address(to_address, user, sizeof(user),
                                       remote, sizeof(remote))) {
        return HYBBX_ERR_INVALID;
    }

    peer = proxy_find_peer_for_service(remote);
    if (peer == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    payload_len = proxy_build_mail_payload(payload, sizeof(payload),
                                           from_address, to_address,
                                           subject != NULL ? subject : "",
                                           body != NULL ? body : "");
    if (payload_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return proxy_peer_send(peer->fd, HYBBX_CIRCUIT_PROTO_PROXY_MAIL,
                           (const uint8_t *)payload, payload_len);
}

hybbx_result_t hybbx_mains_proxy_send_chat(hybbx_service_t *service,
                                           const char *from_address,
                                           const char *line)
{
    char payload[HYBBX_CIRCUIT_MAX_PAYLOAD + 1];
    size_t payload_len;
    unsigned i;
    hybbx_result_t rc = HYBBX_ERR_NOT_FOUND;
    int sent = 0;

    (void)service;

    if (!g_rt.mesh.running || g_rt.live_links == 0 || line == NULL ||
        line[0] == '\0') {
        return HYBBX_ERR_UNSUPPORTED;
    }

    payload_len = proxy_build_chat_payload(payload, sizeof(payload),
                                           from_address, line);
    if (payload_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < g_rt.mesh.peer_count; i++) {
        if (g_rt.peers[i].fd < 0) {
            continue;
        }

        if (proxy_peer_send(g_rt.peers[i].fd, HYBBX_CIRCUIT_PROTO_PROXY_CHAT,
                            (const uint8_t *)payload,
                            payload_len) == HYBBX_OK) {
            sent = 1;
            rc = HYBBX_OK;
        }
    }

    return sent ? rc : HYBBX_ERR_NOT_FOUND;
}

void hybbx_mains_proxy_inbound_frame(hybbx_service_t *service,
                                     hybbx_circuit_proto_t proto,
                                     const uint8_t *payload, size_t len)
{
    if (service == NULL || payload == NULL || len == 0) {
        return;
    }

    if (proto != HYBBX_CIRCUIT_PROTO_PROXY_MAIL &&
        proto != HYBBX_CIRCUIT_PROTO_PROXY_CHAT) {
        return;
    }

    proxy_dispatch_payload(service, proto, payload, len);
}
