/*
 * mains_proxy — Main-to-Main mesh proxy.
 *
 * Peers attach via HBX/Circuit only (circuit_host, circuit_port, link_id).
 * Service linking only — proxymail and proxychat payloads; no user data.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/mains_proxy.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mains_proxy_plugin_state {
    hybbx_service_t *service;
    hybbx_mains_proxy_mesh_t mesh;
    int loaded;
} mains_proxy_plugin_state_t;

static mains_proxy_plugin_state_t g_state;

static hybbx_result_t mains_proxy_parse_instances(const char *config,
                                                  hybbx_mains_proxy_mesh_t *mesh)
{
    const char *cursor;
    const char *end;
    char section_buf[HYBBX_CONFIG_LINE_MAX];
    unsigned count = 0;

    if (mesh == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_mains_proxy_mesh_init(mesh);

    if (config == NULL || config[0] == '\0') {
        return HYBBX_OK;
    }

    cursor = config;
    while (*cursor != '\0' && count < HYBBX_MAINS_PROXY_MAX_PEERS) {
        size_t len;

        end = strchr(cursor, HYBBX_MAINS_PROXY_INSTANCE_SEP);
        if (end == NULL) {
            len = strlen(cursor);
        } else {
            len = (size_t)(end - cursor);
        }

        if (len >= sizeof(section_buf)) {
            return HYBBX_ERR_INVALID;
        }

        memcpy(section_buf, cursor, len);
        section_buf[len] = '\0';

        if (hybbx_mains_proxy_peer_parse(section_buf, &mesh->peers[count]) !=
            HYBBX_OK) {
            return HYBBX_ERR_INVALID;
        }

        if (mesh->peers[count].enabled) {
            count++;
        }

        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }

    mesh->peer_count = count;
    return HYBBX_OK;
}

static hybbx_result_t mains_proxy_init(hybbx_service_t *service)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.service = service;
    hybbx_mains_proxy_mesh_init(&g_state.mesh);
    g_state.loaded = 1;
    return HYBBX_OK;
}

static void mains_proxy_shutdown(void)
{
    hybbx_mains_proxy_mesh_stop(&g_state.mesh);
    memset(&g_state, 0, sizeof(g_state));
}

static hybbx_result_t mains_proxy_start(const char *config)
{
    hybbx_result_t rc;

    if (!g_state.loaded) {
        return HYBBX_ERR_INVALID;
    }

    rc = mains_proxy_parse_instances(config, &g_state.mesh);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[mains_proxy] invalid peer configuration");
        return rc;
    }

    return hybbx_mains_proxy_mesh_start(g_state.service, &g_state.mesh);
}

static hybbx_result_t mains_proxy_stop(void)
{
    hybbx_mains_proxy_mesh_stop(&g_state.mesh);
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_mains_proxy = {
    .name = "mains_proxy",
    .kind = HYBBX_TRANSPORT_MAINS_PROXY,
    .version = 1,
    .init = mains_proxy_init,
    .shutdown = mains_proxy_shutdown,
    .start = mains_proxy_start,
    .stop = mains_proxy_stop,
    .write = NULL,
};

void hybbx_mains_proxy_plugin_tick(void)
{
    if (!g_state.loaded || !g_state.mesh.running) {
        return;
    }

    hybbx_mains_proxy_mesh_tick(g_state.service, &g_state.mesh);
}