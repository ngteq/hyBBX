/*
 * crdop — CB Radio Digital Open Protocol host-client (external ARDOPC/ardopcf).
 * INI: [transport.crdop] or [transport.crdopN]. No embedded modem in HyBBX.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/ardop.h"
#include "hybbx/crdop.h"

#include "../ardop/ardop_link.h"

static const ardop_link_plugin_t g_crdop_link = {
    .name = "crdop",
    .kind = HYBBX_TRANSPORT_CRDOP,
    .default_link_id = "crdop-link",
    .modem_hint = "CRDOPC (ARDOP-compatible host TCP)",
    .parse_config = (ardop_link_config_fn)hybbx_crdop_config_parse,
    .config_free = (void (*)(void *))hybbx_ardop_config_free,
    .config_size = sizeof(hybbx_ardop_config_t),
};

static hybbx_result_t crdop_init(hybbx_service_t *service)
{
    return ardop_link_init(service, &g_crdop_link);
}

static void crdop_shutdown(void)
{
    ardop_link_shutdown(&g_crdop_link);
}

static hybbx_result_t crdop_start(const char *config)
{
    return ardop_link_start(&g_crdop_link, config);
}

static hybbx_result_t crdop_stop(void)
{
    return ardop_link_stop(&g_crdop_link);
}

const hybbx_transport_plugin_t hybbx_plugin_crdop = {
    .name = "crdop",
    .kind = HYBBX_TRANSPORT_CRDOP,
    .version = 1,
    .init = crdop_init,
    .shutdown = crdop_shutdown,
    .start = crdop_start,
    .stop = crdop_stop,
    .write = NULL,
};
