#include "hybbx/tnc.h"
#include "hybbx/log.h"

#include <stdio.h>

const char *hybbx_packet_radio_duplex_name(hybbx_packet_radio_duplex_t duplex)
{
    switch (duplex) {
    case HYBBX_PACKET_RADIO_DUPLEX_FULL:
        return "full";
    case HYBBX_PACKET_RADIO_DUPLEX_HALF:
        return "half";
    case HYBBX_PACKET_RADIO_DUPLEX_UNSET:
    default:
        return "unset";
    }
}

const char *hybbx_packet_radio_band_name(hybbx_packet_radio_band_t band)
{
    switch (band) {
    case HYBBX_PACKET_RADIO_BAND_CB:
        return "cb";
    case HYBBX_PACKET_RADIO_BAND_AMATEUR:
        return "amateur";
    case HYBBX_PACKET_RADIO_BAND_UNSET:
    default:
        return "unset";
    }
}

const char *hybbx_packet_radio_modulation_name(
    hybbx_packet_radio_modulation_t modulation)
{
    switch (modulation) {
    case HYBBX_PACKET_RADIO_MOD_G3RUH_FSK:
        return "g3ruh_fsk";
    case HYBBX_PACKET_RADIO_MOD_AFSK:
        return "afsk";
    case HYBBX_PACKET_RADIO_MOD_UNSET:
    default:
        return "unset";
    }
}

int hybbx_packet_radio_modulation_is_g3ruh(
    hybbx_packet_radio_modulation_t modulation)
{
    return modulation == HYBBX_PACKET_RADIO_MOD_G3RUH_FSK;
}

hybbx_result_t hybbx_tnc_finalize_modulation(hybbx_packet_radio_config_t *cfg)
{
    if (cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cfg->params.modulation == HYBBX_PACKET_RADIO_MOD_UNSET) {
        cfg->params.modulation = HYBBX_PACKET_RADIO_MOD_AFSK;
    }

    if (cfg->params.modulation == HYBBX_PACKET_RADIO_MOD_G3RUH_FSK) {
        cfg->params.modem = HYBBX_TNC2C_MODEM_9600;
        cfg->params.radio_baud = HYBBX_G3RUH_RADIO_BAUD;
        if (cfg->tnc == HYBBX_PACKET_RADIO_TNC_TNC2C &&
            cfg->params.clock_mhz <= HYBBX_TNC2C_CLOCK_4_9_MHZ) {
            cfg->params.clock_mhz = HYBBX_TNC2C_CLOCK_9_8_MHZ;
        }
        cfg->params.txdelay = 15;
        cfg->params.persist = 128;
        cfg->params.slot = 3;
        return HYBBX_OK;
    }

    if (cfg->params.radio_baud == 0) {
        cfg->params.radio_baud = HYBBX_AFSK_RADIO_BAUD;
    }
    if (cfg->params.modem == 0) {
        cfg->params.modem = HYBBX_TNC2C_MODEM_TCM3105;
    }

    return HYBBX_OK;
}

static hybbx_packet_radio_band_t default_band_for_tnc(hybbx_packet_radio_tnc_t tnc)
{
    switch (tnc) {
    case HYBBX_PACKET_RADIO_TNC_PCCOM:
    case HYBBX_PACKET_RADIO_TNC_BAYCOM:
        return HYBBX_PACKET_RADIO_BAND_CB;
    case HYBBX_PACKET_RADIO_TNC_TNC2C:
    case HYBBX_PACKET_RADIO_TNC_TNC2:
    case HYBBX_PACKET_RADIO_TNC_PK232:
    case HYBBX_PACKET_RADIO_TNC_MFJ1278:
    case HYBBX_PACKET_RADIO_TNC_KANTRONICS:
    case HYBBX_PACKET_RADIO_TNC_GENERIC:
    default:
        return HYBBX_PACKET_RADIO_BAND_AMATEUR;
    }
}

const char *hybbx_packet_radio_tnc_name(hybbx_packet_radio_tnc_t tnc)
{
    switch (tnc) {
    case HYBBX_PACKET_RADIO_TNC_BAYCOM:
        return "baycom";
    case HYBBX_PACKET_RADIO_TNC_PCCOM:
        return "pccom";
    case HYBBX_PACKET_RADIO_TNC_GENERIC:
        return "generic";
    case HYBBX_PACKET_RADIO_TNC_TNC2:
        return "tnc2";
    case HYBBX_PACKET_RADIO_TNC_PK232:
        return "pk232";
    case HYBBX_PACKET_RADIO_TNC_MFJ1278:
        return "mfj1278";
    case HYBBX_PACKET_RADIO_TNC_KANTRONICS:
        return "kantronics";
    case HYBBX_PACKET_RADIO_TNC_TNC2C:
    default:
        return "tnc2c";
    }
}

const char *hybbx_kiss_entry_name(hybbx_kiss_entry_t entry)
{
    switch (entry) {
    case HYBBX_KISS_ENTRY_NONE:
        return "none";
    case HYBBX_KISS_ENTRY_KISS_ON:
        return "kiss_on";
    case HYBBX_KISS_ENTRY_ESC_AT_K:
        return "esc_at_k";
    case HYBBX_KISS_ENTRY_AUTO:
        return "auto";
    case HYBBX_KISS_ENTRY_UNSET:
    default:
        return "unset";
    }
}

static hybbx_kiss_entry_t default_kiss_entry_for_tnc(hybbx_packet_radio_tnc_t tnc)
{
    switch (tnc) {
    case HYBBX_PACKET_RADIO_TNC_TNC2C:
    case HYBBX_PACKET_RADIO_TNC_PK232:
    case HYBBX_PACKET_RADIO_TNC_KANTRONICS:
    case HYBBX_PACKET_RADIO_TNC_BAYCOM:
        return HYBBX_KISS_ENTRY_KISS_ON;
    case HYBBX_PACKET_RADIO_TNC_TNC2:
    case HYBBX_PACKET_RADIO_TNC_MFJ1278:
    case HYBBX_PACKET_RADIO_TNC_GENERIC:
        return HYBBX_KISS_ENTRY_AUTO;
    case HYBBX_PACKET_RADIO_TNC_PCCOM:
    default:
        return HYBBX_KISS_ENTRY_NONE;
    }
}

hybbx_result_t hybbx_tnc_finalize_kiss_entry(hybbx_packet_radio_config_t *cfg)
{
    if (cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cfg->params.kiss_entry == HYBBX_KISS_ENTRY_UNSET) {
        cfg->params.kiss_entry = default_kiss_entry_for_tnc(cfg->tnc);
    }

    if (cfg->params.kiss_exit == HYBBX_KISS_EXIT_UNSET) {
        cfg->params.kiss_exit = HYBBX_KISS_EXIT_AUTO;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_tnc_finalize_radio_duplex(hybbx_packet_radio_config_t *cfg)
{
    if (cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cfg->params.band == HYBBX_PACKET_RADIO_BAND_UNSET) {
        cfg->params.band = default_band_for_tnc(cfg->tnc);
    }

    if (cfg->params.duplex == HYBBX_PACKET_RADIO_DUPLEX_UNSET) {
        cfg->params.duplex = HYBBX_PACKET_RADIO_DUPLEX_HALF;
    }

    if (cfg->params.band == HYBBX_PACKET_RADIO_BAND_CB &&
        cfg->params.duplex == HYBBX_PACKET_RADIO_DUPLEX_FULL) {
        hybbx_log_warn("[tnc] radio_band=cb allows half-duplex only; "
                       "ignoring radio_duplex=full");
        cfg->params.duplex = HYBBX_PACKET_RADIO_DUPLEX_HALF;
    }

    cfg->params.fullduplex =
        (cfg->params.duplex == HYBBX_PACKET_RADIO_DUPLEX_FULL) ? 1 : 0;

    return HYBBX_OK;
}

hybbx_result_t hybbx_tnc_finalize_serial_line(hybbx_packet_radio_config_t *cfg)
{
    if (cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cfg->params.data_bits == 0) {
        if (cfg->tnc == HYBBX_PACKET_RADIO_TNC_TNC2C ||
            cfg->tnc == HYBBX_PACKET_RADIO_TNC_MFJ1278) {
            cfg->params.data_bits = HYBBX_TNC2C_DATA_BITS;
        } else {
            cfg->params.data_bits = 8;
        }
    }

    if (cfg->params.serial_parity == HYBBX_TNC_SERIAL_PARITY_UNSET) {
        if (cfg->tnc == HYBBX_PACKET_RADIO_TNC_TNC2C ||
            cfg->tnc == HYBBX_PACKET_RADIO_TNC_MFJ1278) {
            cfg->params.serial_parity = HYBBX_TNC_SERIAL_PARITY_EVEN;
        } else {
            cfg->params.serial_parity = HYBBX_TNC_SERIAL_PARITY_NONE;
        }
    }

    if (cfg->params.stop_bits == 0) {
        cfg->params.stop_bits = 1;
    }

    if (cfg->params.assert_modem_lines < 0) {
        cfg->params.assert_modem_lines =
            (cfg->tnc == HYBBX_PACKET_RADIO_TNC_TNC2C) ? 1 : 0;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_tnc_profile_apply_defaults(hybbx_packet_radio_config_t *cfg)
{
    if (cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (cfg->tnc) {
    case HYBBX_PACKET_RADIO_TNC_BAYCOM:
        if (cfg->baud == 0) {
            cfg->baud = HYBBX_PACKET_RADIO_DEFAULT_BAUD;
        }
        if (cfg->params.band == HYBBX_PACKET_RADIO_BAND_UNSET) {
            cfg->params.band = HYBBX_PACKET_RADIO_BAND_CB;
        }
        cfg->params.radio_baud = 1200;
        cfg->params.modem = HYBBX_TNC2C_MODEM_TCM3105;
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        if (cfg->params.txdelay == 0) {
            cfg->params.txdelay = 30;
        }
        if (cfg->params.persist == 0) {
            cfg->params.persist = 63;
        }
        if (cfg->params.slot == 0) {
            cfg->params.slot = 10;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_PCCOM:
        if (cfg->baud == 0) {
            cfg->baud = HYBBX_PACKET_RADIO_DEFAULT_BAUD;
        }
        if (cfg->params.band == HYBBX_PACKET_RADIO_BAND_UNSET) {
            cfg->params.band = HYBBX_PACKET_RADIO_BAND_CB;
        }
        cfg->params.radio_baud = 1200;
        cfg->params.modem = HYBBX_TNC2C_MODEM_TCM3105;
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_HOSTMODE;
        }
        if (cfg->params.duplex == HYBBX_PACKET_RADIO_DUPLEX_UNSET) {
            cfg->params.duplex = HYBBX_PACKET_RADIO_DUPLEX_HALF;
        }
        if (cfg->params.txdelay == 0) {
            cfg->params.txdelay = 25;
        }
        if (cfg->params.persist == 0) {
            cfg->params.persist = 128;
        }
        if (cfg->params.slot == 0) {
            cfg->params.slot = 10;
        }
        cfg->params.host_connect_on_start = 1;
        break;

    case HYBBX_PACKET_RADIO_TNC_GENERIC:
        if (cfg->baud == 0) {
            cfg->baud = HYBBX_PACKET_RADIO_DEFAULT_BAUD;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_TNC2:
        if (cfg->baud == 0) {
            cfg->baud = 9600;
        }
        if (cfg->params.radio_baud == 0) {
            cfg->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_PK232:
        if (cfg->baud == 0) {
            cfg->baud = 9600;
        }
        if (cfg->params.radio_baud == 0) {
            cfg->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_MFJ1278:
        if (cfg->baud == 0) {
            cfg->baud = 4800;
        }
        if (cfg->params.radio_baud == 0) {
            cfg->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_KANTRONICS:
        if (cfg->baud == 0) {
            cfg->baud = 9600;
        }
        if (cfg->params.radio_baud == 0) {
            cfg->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;

    case HYBBX_PACKET_RADIO_TNC_TNC2C:
    default:
        if (cfg->params.radio_baud == 0) {
            cfg->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
        }
        if (cfg->params.clock_mhz == 0.0f) {
            cfg->params.clock_mhz = HYBBX_TNC2C_CLOCK_4_9_MHZ;
        }
        if (cfg->protocol == 0) {
            cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
        }
        break;
    }

    if (cfg->protocol == 0) {
        cfg->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
    }

    (void)hybbx_tnc_finalize_modulation(cfg);
    (void)hybbx_tnc_finalize_radio_duplex(cfg);
    (void)hybbx_tnc_finalize_serial_line(cfg);
    return hybbx_tnc_finalize_kiss_entry(cfg);
}
