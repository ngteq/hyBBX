#ifndef HYBBX_BAYCOM_H
#define HYBBX_BAYCOM_H

/**
 * BayCom PR-Stack link adapter — Linux kernel HDLC modem (SER12 / PAR96 / EPP)
 * or serial KISS firmware path.
 *
 * Native PR-Stack L2 uses Thomas Sailer kernel drivers (baycom_ser_fdx,
 * baycom_ser_hdx, baycom_par, baycom_epp) and hdlcdrv channel access.
 * See docs/BAYCOM.md.
 */

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default Linux baycom_ser_fdx netdev (SER12 fullduplex). */
#define HYBBX_BAYCOM_DEFAULT_INTERFACE "bcsf0"

/** Default SER12 UART I/O base (COM1). */
#define HYBBX_BAYCOM_DEFAULT_IOBASE 0x3f8u

/** Default SER12 IRQ (COM1). */
#define HYBBX_BAYCOM_DEFAULT_IRQ 4u

/** Default SER12 on-air baud (1200 AFSK). */
#define HYBBX_BAYCOM_DEFAULT_RADIO_BAUD 1200u

typedef enum hybbx_baycom_backend {
    HYBBX_BAYCOM_BACKEND_KERNEL = 1,
    HYBBX_BAYCOM_BACKEND_KISS = 2,
} hybbx_baycom_backend_t;

typedef enum hybbx_baycom_modem_mode {
    HYBBX_BAYCOM_MODE_SER12 = 1,
    HYBBX_BAYCOM_MODE_SER12_SOFTDCD,
    HYBBX_BAYCOM_MODE_SER12_INV,
    HYBBX_BAYCOM_MODE_SER12HDX,
    HYBBX_BAYCOM_MODE_PAR96,
    HYBBX_BAYCOM_MODE_PAR96_SOFTDCD,
    HYBBX_BAYCOM_MODE_EPP,
} hybbx_baycom_modem_mode_t;

typedef struct hybbx_baycom_config {
    hybbx_baycom_backend_t backend;
    hybbx_baycom_modem_mode_t mode;
    char *interface;
    char *kernel_module;
    unsigned iobase;
    unsigned irq;
    unsigned radio_baud;
    int kernel_autoload;
    unsigned txdelay;
    unsigned txtail;
    unsigned slot;
    unsigned persist;
    int full_duplex;
    char *device;
    unsigned serial_baud;
    char *mycall;
    char *circuit_host;
    unsigned circuit_port;
    char *link_id;
    char *link_password;
    char *link_role;
    char *frequency_mhz;
} hybbx_baycom_config_t;

hybbx_result_t hybbx_baycom_config_parse(const char *config,
                                         hybbx_baycom_config_t *out);
void hybbx_baycom_config_free(hybbx_baycom_config_t *config);

const char *hybbx_baycom_modem_mode_name(hybbx_baycom_modem_mode_t mode);
const char *hybbx_baycom_backend_name(hybbx_baycom_backend_t backend);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_BAYCOM_H */
