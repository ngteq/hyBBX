#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/packet_radio.h"
#include "hybbx/tnc2c.h"

#include <stdio.h>
#include <string.h>

static hybbx_service_t *g_service;
static hybbx_session_t *g_session;
static hybbx_packet_radio_config_t g_active_config;
static hybbx_tnc2c_t *g_tnc2c;

extern const hybbx_transport_plugin_t hybbx_plugin_packet_radio;

static const char *device_type_name(hybbx_packet_radio_device_type_t type)
{
    return type == HYBBX_PACKET_RADIO_DEVICE_SERIAL ? "serial" : "usb";
}

static const char *protocol_name(hybbx_packet_radio_protocol_t proto)
{
    return proto == HYBBX_PACKET_RADIO_PROTO_HOSTMODE ? "hostmode" : "kiss";
}

static hybbx_result_t packet_radio_init(hybbx_service_t *service)
{
    g_service = service;
    g_session = NULL;
    g_tnc2c = NULL;
    return HYBBX_OK;
}

static void packet_radio_shutdown(void)
{
    if (g_session != NULL) {
        hybbx_session_close(g_session);
        g_session = NULL;
    }

    if (g_tnc2c != NULL) {
        hybbx_tnc2c_close(g_tnc2c);
        g_tnc2c = NULL;
    }

    hybbx_packet_radio_config_free(&g_active_config);
}

static hybbx_result_t packet_radio_start(const char *config)
{
    hybbx_result_t rc;

    packet_radio_shutdown();
    memset(&g_active_config, 0, sizeof(g_active_config));

    rc = hybbx_packet_radio_config_parse(config, &g_active_config);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[packet_radio] invalid configuration\n");
        return rc;
    }

    printf("[packet_radio] tnc=%s protocol=%s device_type=%s device=%s "
           "host_baud=%u radio_baud=%u\n",
           g_active_config.tnc == HYBBX_PACKET_RADIO_TNC_TNC2C ? "tnc2c" :
                                                                 "generic",
           protocol_name(g_active_config.protocol),
           device_type_name(g_active_config.device_type),
           g_active_config.device,
           g_active_config.baud,
           g_active_config.tnc2c.radio_baud);

    if (g_active_config.tnc == HYBBX_PACKET_RADIO_TNC_TNC2C) {
        rc = hybbx_tnc2c_open(&g_tnc2c, &g_active_config);
        if (rc != HYBBX_OK) {
            packet_radio_shutdown();
            return rc;
        }
    } else {
        fprintf(stderr, "[packet_radio] only TNC2C is implemented currently\n");
        packet_radio_shutdown();
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (g_service != NULL) {
        rc = hybbx_session_open(g_service, &hybbx_plugin_packet_radio,
                                g_tnc2c, &g_session);
        if (rc != HYBBX_OK) {
            fprintf(stderr, "[packet_radio] session open failed\n");
            packet_radio_shutdown();
            return rc;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t packet_radio_stop(void)
{
    printf("[packet_radio] stop\n");
    packet_radio_shutdown();
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_packet_radio = {
    .name = "packet_radio",
    .kind = HYBBX_TRANSPORT_PACKET_RADIO,
    .version = 1,
    .init = packet_radio_init,
    .shutdown = packet_radio_shutdown,
    .start = packet_radio_start,
    .stop = packet_radio_stop,
    .write = NULL,
};
