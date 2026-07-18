#include "hybbx/instance.h"
#include "hybbx/log.h"
#include "hybbx/util.h"

#include <string.h>

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

/*
 * Weak default so tests / tools that link hybbx_core alone behave as Main.
 * Instance binaries override via instance_role_{main,secondary,proxy}.c.
 */
__attribute__((weak)) hybbx_instance_role_t hybbx_instance_role(void)
{
    return HYBBX_INSTANCE_MAIN;
}

const char *hybbx_instance_role_name(void)
{
    switch (hybbx_instance_role()) {
    case HYBBX_INSTANCE_SECONDARY:
        return "Secondary";
    case HYBBX_INSTANCE_PROXY:
        return "Proxy";
    case HYBBX_INSTANCE_MAIN:
    default:
        return "Main";
    }
}

const char *hybbx_instance_binary_name(void)
{
    switch (hybbx_instance_role()) {
    case HYBBX_INSTANCE_SECONDARY:
        return "hybbxsd";
    case HYBBX_INSTANCE_PROXY:
        return "hybbxpd";
    case HYBBX_INSTANCE_MAIN:
    default:
        return "hybbxd";
    }
}

int hybbx_instance_offers_user_bbx(void)
{
    return hybbx_instance_role() == HYBBX_INSTANCE_MAIN;
}

int hybbx_instance_plugin_allowed(const char *plugin_name)
{
    if (plugin_name == NULL || plugin_name[0] == '\0') {
        return 0;
    }

    /* RF / soft-modem transports stay in MAX25-Stack — never in HyBBX. */
    if (str_ieq(plugin_name, "packet_radio") ||
        str_ieq(plugin_name, "baycom") ||
        str_ieq(plugin_name, "ardop") ||
        str_ieq(plugin_name, "crdop")) {
        return 0;
    }

    switch (hybbx_instance_role()) {
    case HYBBX_INSTANCE_MAIN:
        /* Local BBX: WebSocket users + mains_proxy to Secondary peers. */
        return str_ieq(plugin_name, "websocket") ||
               str_ieq(plugin_name, "mains_proxy");

    case HYBBX_INSTANCE_PROXY:
        /* Hybrid TCP bridge — no WebSocket, no RF, no mains_proxy mesh. */
        if (str_ieq(plugin_name, "websocket") ||
            str_ieq(plugin_name, "mains_proxy")) {
            return 0;
        }
        return str_ieq(plugin_name, "telnet") ||
               str_ieq(plugin_name, "ssh");

    case HYBBX_INSTANCE_SECONDARY:
        /* Peer other Mains — TCP transports only; RF via MAX25-Stack. */
        return str_ieq(plugin_name, "telnet") ||
               str_ieq(plugin_name, "ssh") ||
               str_ieq(plugin_name, "mains_proxy");

    default:
        return 0;
    }
}

hybbx_result_t hybbx_networks_enforce_instance(hybbx_networks_config_t *networks)
{
    hybbx_instance_role_t role;

    if (networks == NULL) {
        return HYBBX_ERR_INVALID;
    }

    role = hybbx_instance_role();

    /* Universal: no in-process AX.25 / BayCom / ARDOP / CRDOP. */
    if (networks->ax25) {
        hybbx_log_warn("[instance] %s: forcing ax25=no (RF = MAX25-Stack)",
                       hybbx_instance_role_name());
        networks->ax25 = 0;
    }
    if (networks->baycom) {
        hybbx_log_warn("[instance] %s: forcing baycom=no (RF = MAX25-Stack)",
                       hybbx_instance_role_name());
        networks->baycom = 0;
    }
    if (networks->ardop) {
        hybbx_log_warn("[instance] %s: forcing ardop=no (MAX25-Stack)",
                       hybbx_instance_role_name());
        networks->ardop = 0;
    }
    if (networks->crdop) {
        hybbx_log_warn("[instance] %s: forcing crdop=no (MAX25-Stack)",
                       hybbx_instance_role_name());
        networks->crdop = 0;
    }

    switch (role) {
    case HYBBX_INSTANCE_MAIN:
        networks->telnet = 0;
        networks->ssh = 0;
        networks->websocket = 1;
        networks->circuit = 1;
        networks->mains_proxy = 1;
        hybbx_log_info("[instance] Main: websocket=static circuit=on "
                       "mains_proxy=static (telnet/ssh/RF forced off)");
        break;

    case HYBBX_INSTANCE_SECONDARY:
        networks->websocket = 0;
        networks->circuit = 0;
        networks->mains_proxy = 1;
        hybbx_log_info("[instance] Secondary: mains_proxy=static "
                       "(peer other Mains; no user BBX, no circuit hub)");
        break;

    case HYBBX_INSTANCE_PROXY:
        networks->websocket = 0;
        networks->mains_proxy = 0;
        hybbx_log_info("[instance] Proxy: hybrid TCP bridge "
                       "(no websocket, no RF, no mains_proxy mesh)");
        break;

    default:
        return HYBBX_ERR_INVALID;
    }

    hybbx_log_info("[networks] enforced role=%s telnet=%s ssh=%s ax25=%s "
                   "baycom=%s ardop=%s crdop=%s websocket=%s circuit=%s "
                   "mains_proxy=%s",
                   hybbx_instance_role_name(),
                   hybbx_bool_to_string(networks->telnet),
                   hybbx_bool_to_string(networks->ssh),
                   hybbx_bool_to_string(networks->ax25),
                   hybbx_bool_to_string(networks->baycom),
                   hybbx_bool_to_string(networks->ardop),
                   hybbx_bool_to_string(networks->crdop),
                   hybbx_bool_to_string(networks->websocket),
                   hybbx_bool_to_string(networks->circuit),
                   hybbx_bool_to_string(networks->mains_proxy));

    return HYBBX_OK;
}
