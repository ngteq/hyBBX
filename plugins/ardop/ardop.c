/*
 * ardop — ARDOP host-client link adapter (external ARDOPC / ardopcf).
 * INI: [transport.ardop] or [transport.ardopN].
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/ardop.h"
#include "ardop_link.h"

static const ardop_link_plugin_t g_ardop_link = {
    .name = "ardop",
    .kind = HYBBX_TRANSPORT_ARDOP,
    .default_link_id = "ardop-link",
    .modem_hint = "ARDOPC or ardopcf",
    .parse_config = (ardop_link_config_fn)hybbx_ardop_config_parse,
    .config_free = (void (*)(void *))hybbx_ardop_config_free,
    .config_size = sizeof(hybbx_ardop_config_t),
};

static hybbx_result_t ardop_init(hybbx_service_t *service)
{
    return ardop_link_init(service, &g_ardop_link);
}

static void ardop_shutdown(void)
{
    ardop_link_shutdown(&g_ardop_link);
}

static hybbx_result_t ardop_start(const char *config)
{
    return ardop_link_start(&g_ardop_link, config);
}

static hybbx_result_t ardop_stop(void)
{
    return ardop_link_stop(&g_ardop_link);
}

const hybbx_transport_plugin_t hybbx_plugin_ardop = {
    .name = "ardop",
    .kind = HYBBX_TRANSPORT_ARDOP,
    .version = 1,
    .init = ardop_init,
    .shutdown = ardop_shutdown,
    .start = ardop_start,
    .stop = ardop_stop,
    .write = NULL,
};
