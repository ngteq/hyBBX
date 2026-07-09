#ifndef HYBBX_PACKET_RADIO_H
#define HYBBX_PACKET_RADIO_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default line speed for packet radio (2400 baud). */
#define HYBBX_PACKET_RADIO_DEFAULT_BAUD 2400

/** Default USB AX.25 TNC/modem device (Linux). */
#define HYBBX_PACKET_RADIO_DEFAULT_DEVICE_USB "/dev/ttyUSB0"

#if defined(_WIN32)
#define HYBBX_PACKET_RADIO_DEFAULT_DEVICE_SERIAL "COM1"
#elif defined(__AMIGA__)
#define HYBBX_PACKET_RADIO_DEFAULT_DEVICE_SERIAL "serial.device"
#else
#define HYBBX_PACKET_RADIO_DEFAULT_DEVICE_SERIAL "/dev/ttyS0"
#endif

typedef enum hybbx_packet_radio_device_type {
    HYBBX_PACKET_RADIO_DEVICE_USB = 1,
    HYBBX_PACKET_RADIO_DEVICE_SERIAL = 2
} hybbx_packet_radio_device_type_t;

struct hybbx_packet_radio_config;

/**
 * Parse a semicolon-separated key=value transport config string.
 * Missing keys receive HyBBX defaults (TNC2C, USB, KISS @ 2400 baud).
 * Full config type: include hybbx/tnc.h.
 */
hybbx_result_t hybbx_packet_radio_config_parse(const char *config,
                                               struct hybbx_packet_radio_config *out);

void hybbx_packet_radio_config_free(struct hybbx_packet_radio_config *config);

/**
 * Non-zero when @p config starts a local TNC (device/tnc/protocol/circuit_host).
 * Main bridge-registry rows (link_id only) return zero.
 */
int hybbx_packet_radio_section_is_local_edge(const char *config);

/**
 * Return non-zero when @p baud is valid for packet radio configuration.
 * HyBBX accepts any positive host baud rate; TNC2C documents 300..38400.
 */
int hybbx_packet_radio_baud_valid(unsigned int baud);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PACKET_RADIO_H */
