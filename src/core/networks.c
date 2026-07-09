#include "hybbx/networks.h"
#include "hybbx/config.h"
#include "hybbx/util.h"

#include <stdio.h>
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

static int networks_has_key(const hybbx_config_t *config, const char *key)
{
    const char *value;

    if (config == NULL || key == NULL) {
        return 0;
    }

    value = hybbx_config_get(config, "networks", key, NULL);
    return value != NULL && value[0] != '\0';
}

void hybbx_networks_config_defaults(hybbx_networks_config_t *networks)
{
    if (networks == NULL) {
        return;
    }

    networks->ax25 = 0;
    networks->baycom = 0;
    networks->ardop = 0;
    networks->crdop = 0;
    networks->ssh = 0;
    networks->websocket = 0;
    networks->circuit = 1;
    networks->mains_proxy = 0;
}

void hybbx_networks_config_apply(hybbx_networks_config_t *networks,
                                 const hybbx_config_t *config)
{
    if (networks == NULL) {
        return;
    }

    hybbx_networks_config_defaults(networks);

    if (config == NULL) {
        return;
    }

    networks->ax25 = hybbx_config_get_bool(config, "networks", "ax25", 0);

    if (networks_has_key(config, "baycom")) {
        networks->baycom = hybbx_config_get_bool(config, "networks", "baycom", 0);
    }

    if (networks_has_key(config, "ardop")) {
        networks->ardop = hybbx_config_get_bool(config, "networks", "ardop", 0);
    }

    if (networks_has_key(config, "crdop")) {
        networks->crdop = hybbx_config_get_bool(config, "networks", "crdop", 0);
    }

    networks->ssh = hybbx_config_get_bool(config, "networks", "ssh", 0);

    networks->websocket = hybbx_config_get_bool(config, "networks",
                                                "websocket", 0);

    if (networks_has_key(config, "circuit")) {
        networks->circuit = hybbx_config_get_bool(config, "networks",
                                                  "circuit", 1);
    } else {
        networks->circuit = hybbx_config_get_bool(config, "circuit", "enabled",
                                                  1);
    }

    if (networks_has_key(config, "mains_proxy")) {
        networks->mains_proxy = hybbx_config_get_bool(config, "networks",
                                                        "mains_proxy", 0);
    }

    printf("[networks] telnet=static ssh=%s ax25=%s baycom=%s ardop=%s crdop=%s "
           "websocket=%s circuit=%s mains_proxy=%s\n",
           hybbx_bool_to_string(networks->ssh),
           hybbx_bool_to_string(networks->ax25),
           hybbx_bool_to_string(networks->baycom),
           hybbx_bool_to_string(networks->ardop),
           hybbx_bool_to_string(networks->crdop),
           hybbx_bool_to_string(networks->websocket),
           hybbx_bool_to_string(networks->circuit),
           hybbx_bool_to_string(networks->mains_proxy));
}

int hybbx_networks_is_static_transport(const char *plugin_name)
{
    if (plugin_name == NULL || plugin_name[0] == '\0') {
        return 0;
    }

    return str_ieq(plugin_name, "telnet");
}

int hybbx_networks_transport_wanted(const char *plugin_name,
                                    const hybbx_networks_config_t *networks)
{
    if (plugin_name == NULL || plugin_name[0] == '\0') {
        return 0;
    }

    if (hybbx_networks_is_static_transport(plugin_name)) {
        return 1;
    }

    if (networks == NULL) {
        return 0;
    }

    if (str_ieq(plugin_name, "packet_radio")) {
        return networks->ax25;
    }

    if (str_ieq(plugin_name, "baycom")) {
        return networks->baycom;
    }

    if (str_ieq(plugin_name, "ardop")) {
        return networks->ardop;
    }

    if (str_ieq(plugin_name, "crdop")) {
        return networks->crdop;
    }

    if (str_ieq(plugin_name, "ssh")) {
        return networks->ssh;
    }

    if (str_ieq(plugin_name, "websocket")) {
        return networks->websocket;
    }

    if (str_ieq(plugin_name, "mains_proxy")) {
        return networks->mains_proxy;
    }

    return 0;
}
