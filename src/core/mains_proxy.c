#include "hybbx/mains_proxy.h"
#include "hybbx/service.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Inter-node mesh: hybbx_circuit_link_connect() only — never peer Main TCP. */

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
        fprintf(stderr,
                "[mains_proxy] peer '%s': deprecated key host= — use "
                "circuit_host= (mapped for now)\n",
                out->peer_id[0] != '\0' ? out->peer_id : "(unnamed)");
    } else if (legacy_host) {
        fprintf(stderr,
                "[mains_proxy] peer '%s': deprecated key host= ignored "
                "(circuit_host set)\n",
                out->peer_id[0] != '\0' ? out->peer_id : "(unnamed)");
    }

    if (legacy_port) {
        fprintf(stderr,
                "[mains_proxy] peer '%s': deprecated key port= ignored — "
                "use circuit_port= (HBX hub, default %u)\n",
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

static hybbx_result_t mains_proxy_peer_connect_stub(
    const hybbx_mains_proxy_peer_config_t *peer)
{
    const char *peer_label;

    if (peer == NULL || !peer->enabled) {
        return HYBBX_OK;
    }

    peer_label = peer->peer_id[0] != '\0' ? peer->peer_id : "(unnamed)";

    if (peer->circuit_host[0] == '\0') {
        fprintf(stderr,
                "[mains_proxy] peer '%s' missing circuit_host — skipped\n",
                peer_label);
        return HYBBX_ERR_INVALID;
    }

    if (peer->link_id[0] == '\0') {
        fprintf(stderr,
                "[mains_proxy] peer '%s' missing link_id — skipped\n",
                peer_label);
        return HYBBX_ERR_INVALID;
    }

    printf("[mains_proxy] would link via HBX circuit %s:%u "
           "link_id=%s peer=%s wire=%s duplex=%s secondary=%s "
           "(relay not active yet)\n",
           peer->circuit_host, peer->circuit_port, peer->link_id, peer_label,
           hybbx_mains_proxy_wire_name(peer->wire),
           hybbx_mains_proxy_duplex_name(peer->duplex),
           peer->use_secondary ? "yes" : "no");

    return HYBBX_OK;
}

hybbx_result_t hybbx_mains_proxy_mesh_start(hybbx_service_t *service,
                                            hybbx_mains_proxy_mesh_t *mesh)
{
    unsigned i;
    unsigned active = 0;

    (void)service;

    if (mesh == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_mains_proxy_mesh_stop(mesh);

    for (i = 0; i < mesh->peer_count; i++) {
        if (!mesh->peers[i].enabled) {
            continue;
        }

        if (mains_proxy_peer_connect_stub(&mesh->peers[i]) == HYBBX_OK) {
            active++;
        }
    }

    if (active == 0) {
        printf("[mains_proxy] no active peers configured\n");
        return HYBBX_ERR_NOT_FOUND;
    }

    mesh->running = 1;
    printf("[mains_proxy] mesh started (%u peer(s); HBX circuit relay "
           "not active yet)\n", active);

    return HYBBX_OK;
}

void hybbx_mains_proxy_mesh_stop(hybbx_mains_proxy_mesh_t *mesh)
{
    if (mesh == NULL) {
        return;
    }

    if (mesh->running) {
        printf("[mains_proxy] mesh stopped\n");
    }

    mesh->running = 0;
}

void hybbx_mains_proxy_mesh_tick(hybbx_service_t *service,
                                 hybbx_mains_proxy_mesh_t *mesh)
{
    (void)service;

    if (mesh == NULL || !mesh->running) {
        return;
    }

    /* Stub: mesh relay I/O via HBX circuit frames will run here. */
}
