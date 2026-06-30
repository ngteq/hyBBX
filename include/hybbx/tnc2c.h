#ifndef HYBBX_TNC2C_H
#define HYBBX_TNC2C_H

/**
 * HyBBX driver for the Landolt TNC2C Packet Radio Controller.
 *
 * Reference: https://www.landolt.de/info/afuinfo/tnc2c.htm
 *
 * TNC2C features relevant to HyBBX:
 *  - RS232 (V.24) host interface via MAX232
 *  - KISS and host-mode firmware (EPROM)
 *  - TCM 3105 modem (1200 baud radio default)
 *  - Optional 9600/19200 baud modems (switchable, e.g. TNC2C-H)
 *  - Host/terminal speeds 300..9600 baud (up to 38400 on 10 MHz version)
 *  - 32 kB RAM for offline message storage
 *  - Watchdog and standalone mailbox operation
 */

#include "hybbx/packet_radio.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default TNC2C host serial speed (HyBBX default). */
#define HYBBX_TNC2C_DEFAULT_HOST_BAUD HYBBX_PACKET_RADIO_DEFAULT_BAUD

/** TCM 3105 standard over-the-air speed. */
#define HYBBX_TNC2C_DEFAULT_RADIO_BAUD 1200u

/** Standard TNC2C crystal clock (MHz). */
#define HYBBX_TNC2C_CLOCK_4_9_MHZ  4.9f
#define HYBBX_TNC2C_CLOCK_9_8_MHZ  9.8f
#define HYBBX_TNC2C_CLOCK_10_MHZ   10.0f

/** Documented host speed range (baud). */
#define HYBBX_TNC2C_HOST_BAUD_MIN  300u
#define HYBBX_TNC2C_HOST_BAUD_MAX  38400u

typedef enum hybbx_tnc2c_modem {
    HYBBX_TNC2C_MODEM_TCM3105 = 1,
    HYBBX_TNC2C_MODEM_9600 = 2,
    HYBBX_TNC2C_MODEM_19200 = 3
} hybbx_tnc2c_modem_t;

typedef enum hybbx_packet_radio_protocol {
    HYBBX_PACKET_RADIO_PROTO_KISS = 1,
    HYBBX_PACKET_RADIO_PROTO_HOSTMODE = 2
} hybbx_packet_radio_protocol_t;

typedef enum hybbx_packet_radio_tnc {
    HYBBX_PACKET_RADIO_TNC_TNC2C = 1,
    HYBBX_PACKET_RADIO_TNC_GENERIC = 2
} hybbx_packet_radio_tnc_t;

typedef struct hybbx_tnc2c_params {
    float clock_mhz;
    hybbx_tnc2c_modem_t modem;
    unsigned int radio_baud;
    unsigned int kiss_port;
    unsigned int txdelay;
    unsigned int persist;
    unsigned int slot;
    unsigned int txtail;
    int fullduplex;
    int kiss_on_startup;
} hybbx_tnc2c_params_t;

struct hybbx_packet_radio_config {
    hybbx_packet_radio_device_type_t device_type;
    char *device;
    unsigned int baud;
    hybbx_packet_radio_tnc_t tnc;
    hybbx_packet_radio_protocol_t protocol;
    hybbx_tnc2c_params_t tnc2c;
};

typedef struct hybbx_tnc2c hybbx_tnc2c_t;

hybbx_result_t hybbx_tnc2c_open(hybbx_tnc2c_t **out,
                                  const hybbx_packet_radio_config_t *config);

void hybbx_tnc2c_close(hybbx_tnc2c_t *tnc);

hybbx_result_t hybbx_tnc2c_send_frame(hybbx_tnc2c_t *tnc,
                                      const uint8_t *frame, size_t len);

hybbx_result_t hybbx_tnc2c_poll(hybbx_tnc2c_t *tnc);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TNC2C_H */
