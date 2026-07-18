#include "hybbx/instance.h"
#include "hybbx/networks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect_int(const char *label, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", label, got, want);
        failures++;
    }
}

static void test_enforce(void)
{
    hybbx_networks_config_t net;

    hybbx_networks_config_defaults(&net);
    net.telnet = 1;
    net.ax25 = 1;
    net.baycom = 1;
    net.ardop = 1;
    net.crdop = 1;
    net.websocket = 0;
    net.mains_proxy = 0;

    if (hybbx_networks_enforce_instance(&net) != HYBBX_OK) {
        fprintf(stderr, "FAIL enforce returned error\n");
        failures++;
        return;
    }

    expect_int("ax25 off", net.ax25, 0);
    expect_int("baycom off", net.baycom, 0);
    expect_int("ardop off", net.ardop, 0);
    expect_int("crdop off", net.crdop, 0);

    switch (hybbx_instance_role()) {
    case HYBBX_INSTANCE_MAIN:
        expect_int("main telnet", net.telnet, 0);
        expect_int("main websocket", net.websocket, 1);
        expect_int("main circuit", net.circuit, 1);
        expect_int("main mains_proxy", net.mains_proxy, 1);
        break;
    case HYBBX_INSTANCE_SECONDARY:
        expect_int("secondary websocket", net.websocket, 0);
        expect_int("secondary circuit", net.circuit, 0);
        expect_int("secondary mains_proxy", net.mains_proxy, 1);
        break;
    case HYBBX_INSTANCE_PROXY:
        expect_int("proxy websocket", net.websocket, 0);
        expect_int("proxy mains_proxy", net.mains_proxy, 0);
        break;
    default:
        fprintf(stderr, "FAIL unknown role\n");
        failures++;
        break;
    }
}

static void test_plugin_allow(void)
{
    expect_int("packet_radio denied",
               hybbx_instance_plugin_allowed("packet_radio"), 0);
    expect_int("baycom denied",
               hybbx_instance_plugin_allowed("baycom"), 0);
    expect_int("ardop denied",
               hybbx_instance_plugin_allowed("ardop"), 0);
    expect_int("crdop denied",
               hybbx_instance_plugin_allowed("crdop"), 0);

    switch (hybbx_instance_role()) {
    case HYBBX_INSTANCE_MAIN:
        expect_int("main websocket", hybbx_instance_plugin_allowed("websocket"), 1);
        expect_int("main mains_proxy", hybbx_instance_plugin_allowed("mains_proxy"), 1);
        expect_int("main telnet", hybbx_instance_plugin_allowed("telnet"), 0);
        expect_int("main user_bbx", hybbx_instance_offers_user_bbx(), 1);
        break;
    case HYBBX_INSTANCE_SECONDARY:
        expect_int("secondary mains_proxy",
                   hybbx_instance_plugin_allowed("mains_proxy"), 1);
        expect_int("secondary telnet", hybbx_instance_plugin_allowed("telnet"), 1);
        expect_int("secondary websocket",
                   hybbx_instance_plugin_allowed("websocket"), 0);
        expect_int("secondary user_bbx", hybbx_instance_offers_user_bbx(), 0);
        break;
    case HYBBX_INSTANCE_PROXY:
        expect_int("proxy telnet", hybbx_instance_plugin_allowed("telnet"), 1);
        expect_int("proxy ssh", hybbx_instance_plugin_allowed("ssh"), 1);
        expect_int("proxy websocket", hybbx_instance_plugin_allowed("websocket"), 0);
        expect_int("proxy mains_proxy", hybbx_instance_plugin_allowed("mains_proxy"), 0);
        expect_int("proxy user_bbx", hybbx_instance_offers_user_bbx(), 0);
        break;
    default:
        break;
    }
}

int main(void)
{
    printf("instance tests role=%s binary=%s\n",
           hybbx_instance_role_name(),
           hybbx_instance_binary_name());
    test_plugin_allow();
    test_enforce();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
