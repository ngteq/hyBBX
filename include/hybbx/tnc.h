#ifndef HYBBX_TNC_H
#define HYBBX_TNC_H

/**
 * Unified TNC / AX.25 link driver for HyBBX packet radio.
 *
 * Supported hardware profiles (see docs/TNCS.md):
 *  - Landolt TNC2C, classic TNC-2 / PK-TNC2 (TheFirmware)
 *  - AEA PK-232, MFJ-1278, Kantronics KPC
 *  - BayCom-compatible modems (KISS / host / 6PACK over RS-232)
 *  - Albrecht PC-COM (2400 baud host, TCM3105 1200 radio)
 *  - Generic TNC2-class controllers
 *
 * Host protocols: KISS, TNC2 host mode, 6PACK (DF6BU).
 */

#include "hybbx/ax25.h"
#include "hybbx/packet_radio.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_TNC2C_DEFAULT_HOST_BAUD HYBBX_PACKET_RADIO_DEFAULT_BAUD
#define HYBBX_TNC2C_DEFAULT_RADIO_BAUD 1200u
#define HYBBX_AFSK_RADIO_BAUD          1200u
#define HYBBX_G3RUH_RADIO_BAUD         9600u
#define HYBBX_TNC2C_CLOCK_4_9_MHZ  4.9f
#define HYBBX_TNC2C_CLOCK_9_8_MHZ  9.8f
#define HYBBX_TNC2C_CLOCK_10_MHZ   10.0f
#define HYBBX_TNC2C_HOST_BAUD_MIN  300u
#define HYBBX_TNC2C_HOST_BAUD_MAX  38400u
#define HYBBX_TNC2C_DATA_BITS      7u

typedef enum hybbx_tnc_serial_parity {
    HYBBX_TNC_SERIAL_PARITY_UNSET = 0,
    HYBBX_TNC_SERIAL_PARITY_NONE = 1,
    HYBBX_TNC_SERIAL_PARITY_EVEN = 2,
    HYBBX_TNC_SERIAL_PARITY_ODD = 3
} hybbx_tnc_serial_parity_t;

#define HYBBX_PACKET_RADIO_VIA_MAX 8

typedef enum hybbx_tnc2c_modem {
    HYBBX_TNC2C_MODEM_TCM3105 = 1,
    HYBBX_TNC2C_MODEM_9600 = 2,   /**< G3RUH-compatible 9600 baud FSK */
    HYBBX_TNC2C_MODEM_19200 = 3
} hybbx_tnc2c_modem_t;

/**
 * Over-the-air modulation standard.
 * AFSK (Bell 202 / TCM3105 @ 1200) is the classic CB packet default.
 * G3RUH FSK @ 9600 baud for higher throughput (requires compatible modem/TNC).
 */
typedef enum hybbx_packet_radio_modulation {
    HYBBX_PACKET_RADIO_MOD_UNSET = 0,
    HYBBX_PACKET_RADIO_MOD_AFSK = 1,
    HYBBX_PACKET_RADIO_MOD_G3RUH_FSK = 2
} hybbx_packet_radio_modulation_t;

#define HYBBX_PACKET_RADIO_MOD_DEFAULT HYBBX_PACKET_RADIO_MOD_AFSK

typedef enum hybbx_packet_radio_protocol {
    HYBBX_PACKET_RADIO_PROTO_KISS = 1,
    HYBBX_PACKET_RADIO_PROTO_HOSTMODE = 2,
    HYBBX_PACKET_RADIO_PROTO_SIXPACK = 3
} hybbx_packet_radio_protocol_t;

typedef enum hybbx_packet_radio_tnc {
    HYBBX_PACKET_RADIO_TNC_TNC2C = 1,
    HYBBX_PACKET_RADIO_TNC_BAYCOM = 2,
    HYBBX_PACKET_RADIO_TNC_PCCOM = 3,
    HYBBX_PACKET_RADIO_TNC_GENERIC = 4,
    HYBBX_PACKET_RADIO_TNC_TNC2 = 5,
    HYBBX_PACKET_RADIO_TNC_PK232 = 6,
    HYBBX_PACKET_RADIO_TNC_MFJ1278 = 7,
    HYBBX_PACKET_RADIO_TNC_KANTRONICS = 8
} hybbx_packet_radio_tnc_t;

/** How HyBBX enters KISS mode on the host serial link. */
typedef enum hybbx_kiss_entry {
    HYBBX_KISS_ENTRY_UNSET = 0,
    HYBBX_KISS_ENTRY_NONE = 1,
    HYBBX_KISS_ENTRY_KISS_ON = 2,
    HYBBX_KISS_ENTRY_ESC_AT_K = 3,
    HYBBX_KISS_ENTRY_AUTO = 4
} hybbx_kiss_entry_t;

/** How HyBBX leaves KISS mode on shutdown. */
typedef enum hybbx_kiss_exit {
    HYBBX_KISS_EXIT_UNSET = 0,
    HYBBX_KISS_EXIT_NONE = 1,
    HYBBX_KISS_EXIT_KISS_OFF = 2,
    HYBBX_KISS_EXIT_KISS_FRAME = 3,
    HYBBX_KISS_EXIT_AUTO = 4
} hybbx_kiss_exit_t;

/**
 * RF service band — drives safe duplex defaults.
 * CB is half-duplex only (shared channel). Amateur may use full-duplex.
 */
typedef enum hybbx_packet_radio_band {
    HYBBX_PACKET_RADIO_BAND_UNSET = 0,
    HYBBX_PACKET_RADIO_BAND_CB = 1,
    HYBBX_PACKET_RADIO_BAND_AMATEUR = 2
} hybbx_packet_radio_band_t;

/**
 * Radio link duplex mode (distinct from host serial full-duplex).
 * half = one direction at a time on the RF channel (CB / classic packet).
 * full = TNC may key TX while receiving (amateur split / full-dup setups).
 */
typedef enum hybbx_packet_radio_duplex {
    HYBBX_PACKET_RADIO_DUPLEX_UNSET = 0,
    HYBBX_PACKET_RADIO_DUPLEX_HALF = 1,
    HYBBX_PACKET_RADIO_DUPLEX_FULL = 2
} hybbx_packet_radio_duplex_t;

#define HYBBX_PACKET_RADIO_DUPLEX_DEFAULT HYBBX_PACKET_RADIO_DUPLEX_HALF

typedef struct hybbx_tnc_params {
    float clock_mhz;
    hybbx_tnc2c_modem_t modem;
    hybbx_packet_radio_modulation_t modulation;
    unsigned int radio_baud;
    unsigned int kiss_port;
    unsigned int txdelay;
    unsigned int persist;
    unsigned int slot;
    unsigned int txtail;
    hybbx_packet_radio_band_t band;
    hybbx_packet_radio_duplex_t duplex;
    int fullduplex;
    hybbx_kiss_entry_t kiss_entry;
    hybbx_kiss_exit_t kiss_exit;
    int host_connect_on_start;
    unsigned int data_bits;
    hybbx_tnc_serial_parity_t serial_parity;
    unsigned int stop_bits;
    int assert_modem_lines;
} hybbx_tnc_params_t;

typedef struct hybbx_packet_radio_config {
    hybbx_packet_radio_device_type_t device_type;
    char *device;
    unsigned int baud;
    hybbx_packet_radio_tnc_t tnc;
    hybbx_packet_radio_protocol_t protocol;
    hybbx_tnc_params_t params;
    char *mycall;
    char *dest_call;
    char *via[HYBBX_PACKET_RADIO_VIA_MAX];
    unsigned via_count;
    char *circuit_host;
    unsigned circuit_port;
    char *link_id;
    char *link_password;
    char *link_role;
    char *frequency_mhz;
} hybbx_packet_radio_config_t;

typedef struct hybbx_tnc hybbx_tnc_t;

typedef void (*hybbx_tnc_ui_cb)(const uint8_t *payload, size_t len,
                                const hybbx_ax25_path_t *path,
                                void *userdata);

/** Raw AX.25 frame from KISS/6PACK (before UI parsing). */
typedef void (*hybbx_tnc_frame_cb)(const uint8_t *frame, size_t len,
                                   void *userdata);

hybbx_result_t hybbx_tnc_open(hybbx_tnc_t **out,
                              const hybbx_packet_radio_config_t *config,
                              hybbx_tnc_frame_cb frame_cb,
                              hybbx_tnc_ui_cb ui_cb,
                              void *rx_userdata);

void hybbx_tnc_close(hybbx_tnc_t *tnc);

hybbx_result_t hybbx_tnc_poll(hybbx_tnc_t *tnc);

/** Send a raw AX.25 frame (already built) to the TNC. */
hybbx_result_t hybbx_tnc_send_frame(hybbx_tnc_t *tnc,
                                    const uint8_t *frame, size_t len);

/** Build and send an AX.25 UI frame from text payload. */
hybbx_result_t hybbx_tnc_send_ui(hybbx_tnc_t *tnc,
                                 const uint8_t *payload, size_t len);

/** Send bytes during host-mode connected converse. */
hybbx_result_t hybbx_tnc_send_converse(hybbx_tnc_t *tnc,
                                       const char *data, size_t len);

int hybbx_tnc_host_connected(const hybbx_tnc_t *tnc);

hybbx_packet_radio_duplex_t hybbx_tnc_duplex(const hybbx_tnc_t *tnc);
hybbx_packet_radio_band_t hybbx_tnc_band(const hybbx_tnc_t *tnc);
int hybbx_tnc_duplex_is_full(const hybbx_tnc_t *tnc);

const char *hybbx_packet_radio_duplex_name(hybbx_packet_radio_duplex_t duplex);
const char *hybbx_packet_radio_band_name(hybbx_packet_radio_band_t band);

const char *hybbx_packet_radio_modulation_name(
    hybbx_packet_radio_modulation_t modulation);

int hybbx_packet_radio_modulation_is_g3ruh(
    hybbx_packet_radio_modulation_t modulation);

/** Apply g3ruh_fsk / modulation keys to modem, radio_baud, and timing. */
hybbx_result_t hybbx_tnc_finalize_modulation(hybbx_packet_radio_config_t *cfg);

/** Apply band/duplex rules and sync @p fullduplex for the TNC firmware. */
hybbx_result_t hybbx_tnc_finalize_radio_duplex(hybbx_packet_radio_config_t *cfg);

/** Apply profile-specific defaults (TNC2C, BayCom, PC-COM, generic). */
hybbx_result_t hybbx_tnc_profile_apply_defaults(hybbx_packet_radio_config_t *cfg);

/** Apply host serial line defaults (TNC2C: 7E1 + RTS/DTR). */
hybbx_result_t hybbx_tnc_finalize_serial_line(hybbx_packet_radio_config_t *cfg);

/** Apply kiss_entry / kiss_exit defaults from TNC profile when unset. */
hybbx_result_t hybbx_tnc_finalize_kiss_entry(hybbx_packet_radio_config_t *cfg);

const char *hybbx_packet_radio_tnc_name(hybbx_packet_radio_tnc_t tnc);
const char *hybbx_kiss_entry_name(hybbx_kiss_entry_t entry);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TNC_H */
