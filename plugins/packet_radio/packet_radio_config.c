#include "hybbx/packet_radio.h"
#include "hybbx/tnc.h"
#include "hybbx/circuit.h"
#include "hybbx/util.h"

#include <stdlib.h>
#include <string.h>

static char *hybbx_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

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

static const char *find_kv(const char *config, const char *key,
                           char *scratch, size_t scratch_len)
{
    const char *cursor = config;
    size_t key_len = strlen(key);

    if (config == NULL || key == NULL) {
        return NULL;
    }

    while (*cursor != '\0') {
        const char *sep = strchr(cursor, ';');
        const char *end = sep != NULL ? sep : cursor + strlen(cursor);
        const char *eq = strchr(cursor, '=');

        if (eq != NULL && eq < end && (size_t)(eq - cursor) == key_len &&
            strncmp(cursor, key, key_len) == 0) {
            const char *value = eq + 1;
            size_t value_len = (size_t)(end - value);

            if (value_len >= scratch_len) {
                value_len = scratch_len - 1;
            }
            memcpy(scratch, value, value_len);
            scratch[value_len] = '\0';
            return scratch;
        }

        if (sep == NULL) {
            break;
        }
        cursor = sep + 1;
    }

    return NULL;
}

int hybbx_packet_radio_section_is_local_edge(const char *config)
{
    char scratch[64];

    if (config == NULL) {
        return 0;
    }

    if (find_kv(config, "circuit_host", scratch, sizeof(scratch)) != NULL) {
        return 1;
    }
    if (find_kv(config, "device", scratch, sizeof(scratch)) != NULL) {
        return 1;
    }
    if (find_kv(config, "tnc", scratch, sizeof(scratch)) != NULL) {
        return 1;
    }
    if (find_kv(config, "protocol", scratch, sizeof(scratch)) != NULL) {
        return 1;
    }

    return 0;
}

static hybbx_packet_radio_device_type_t parse_device_type(const char *value)
{
    if (value == NULL) {
        return HYBBX_PACKET_RADIO_DEVICE_USB;
    }

    if (str_ieq(value, "usb") || str_ieq(value, "ax25") ||
        str_ieq(value, "ax.25")) {
        return HYBBX_PACKET_RADIO_DEVICE_USB;
    }

    if (str_ieq(value, "serial") || str_ieq(value, "com") ||
        str_ieq(value, "rs232") || str_ieq(value, "v24")) {
        return HYBBX_PACKET_RADIO_DEVICE_SERIAL;
    }

    return HYBBX_PACKET_RADIO_DEVICE_USB;
}

static const char *default_device_for_type(hybbx_packet_radio_device_type_t type)
{
    if (type == HYBBX_PACKET_RADIO_DEVICE_SERIAL) {
        return HYBBX_PACKET_RADIO_DEFAULT_DEVICE_SERIAL;
    }
    return HYBBX_PACKET_RADIO_DEFAULT_DEVICE_USB;
}

static hybbx_packet_radio_tnc_t parse_tnc(const char *value)
{
    if (value == NULL || str_ieq(value, "tnc2c") || str_ieq(value, "tnc2-c")) {
        return HYBBX_PACKET_RADIO_TNC_TNC2C;
    }

    if (str_ieq(value, "tnc2") || str_ieq(value, "tnc-2") ||
        str_ieq(value, "pktnc2") || str_ieq(value, "pk-tnc2") ||
        str_ieq(value, "tapr") || str_ieq(value, "thefirmware") ||
        str_ieq(value, "tf")) {
        return HYBBX_PACKET_RADIO_TNC_TNC2;
    }

    if (str_ieq(value, "pk232") || str_ieq(value, "pk-232") ||
        str_ieq(value, "aea") || str_ieq(value, "pk87") ||
        str_ieq(value, "pk-87")) {
        return HYBBX_PACKET_RADIO_TNC_PK232;
    }

    if (str_ieq(value, "mfj1278") || str_ieq(value, "mfj-1278") ||
        str_ieq(value, "mfj1278b") || str_ieq(value, "mfj")) {
        return HYBBX_PACKET_RADIO_TNC_MFJ1278;
    }

    if (str_ieq(value, "kantronics") || str_ieq(value, "kpc") ||
        str_ieq(value, "kpc3") || str_ieq(value, "kpc9612")) {
        return HYBBX_PACKET_RADIO_TNC_KANTRONICS;
    }

    if (str_ieq(value, "baycom") || str_ieq(value, "bay-com")) {
        return HYBBX_PACKET_RADIO_TNC_BAYCOM;
    }

    if (str_ieq(value, "pccom") || str_ieq(value, "pc-com") ||
        str_ieq(value, "albrecht")) {
        return HYBBX_PACKET_RADIO_TNC_PCCOM;
    }

    if (str_ieq(value, "generic") || str_ieq(value, "tnc")) {
        return HYBBX_PACKET_RADIO_TNC_GENERIC;
    }

    return HYBBX_PACKET_RADIO_TNC_TNC2C;
}

static hybbx_packet_radio_protocol_t parse_protocol(const char *value)
{
    if (value == NULL || str_ieq(value, "kiss")) {
        return HYBBX_PACKET_RADIO_PROTO_KISS;
    }

    if (str_ieq(value, "hostmode") || str_ieq(value, "host") ||
        str_ieq(value, "tnc2")) {
        return HYBBX_PACKET_RADIO_PROTO_HOSTMODE;
    }

    if (str_ieq(value, "6pack") || str_ieq(value, "sixpack")) {
        return HYBBX_PACKET_RADIO_PROTO_SIXPACK;
    }

    return HYBBX_PACKET_RADIO_PROTO_KISS;
}

static hybbx_tnc2c_modem_t parse_modem(const char *value)
{
    if (value == NULL || str_ieq(value, "tcm3105") || str_ieq(value, "1200")) {
        return HYBBX_TNC2C_MODEM_TCM3105;
    }

    if (str_ieq(value, "9600")) {
        return HYBBX_TNC2C_MODEM_9600;
    }

    if (str_ieq(value, "g3ruh") || str_ieq(value, "g3ruh_fsk") ||
        str_ieq(value, "fsk9600")) {
        return HYBBX_TNC2C_MODEM_9600;
    }

    if (str_ieq(value, "19200")) {
        return HYBBX_TNC2C_MODEM_19200;
    }

    return HYBBX_TNC2C_MODEM_TCM3105;
}

static float parse_clock_mhz(const char *value)
{
    if (value == NULL) {
        return HYBBX_TNC2C_CLOCK_4_9_MHZ;
    }

    if (str_ieq(value, "9.8") || str_ieq(value, "9,8")) {
        return HYBBX_TNC2C_CLOCK_9_8_MHZ;
    }

    if (str_ieq(value, "10") || str_ieq(value, "10.0")) {
        return HYBBX_TNC2C_CLOCK_10_MHZ;
    }

    return HYBBX_TNC2C_CLOCK_4_9_MHZ;
}

static unsigned int parse_uint(const char *value, unsigned int default_value)
{
    char *end = NULL;
    unsigned long n;

    if (value == NULL || value[0] == '\0') {
        return default_value;
    }

    n = strtoul(value, &end, 10);
    if (end == value || n == 0) {
        return default_value;
    }

    return (unsigned int)n;
}

static hybbx_packet_radio_band_t parse_radio_band(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_PACKET_RADIO_BAND_UNSET;
    }

    if (str_ieq(value, "cb") || str_ieq(value, "11m") ||
        str_ieq(value, "27mhz") || str_ieq(value, "citizen")) {
        return HYBBX_PACKET_RADIO_BAND_CB;
    }

    if (str_ieq(value, "amateur") || str_ieq(value, "ham") ||
        str_ieq(value, "amateurfunk") || str_ieq(value, "afu")) {
        return HYBBX_PACKET_RADIO_BAND_AMATEUR;
    }

    return HYBBX_PACKET_RADIO_BAND_UNSET;
}

static hybbx_packet_radio_duplex_t parse_radio_duplex(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_PACKET_RADIO_DUPLEX_UNSET;
    }

    if (str_ieq(value, "full") || str_ieq(value, "full-duplex") ||
        str_ieq(value, "fullduplex") || str_ieq(value, "fulldup")) {
        return HYBBX_PACKET_RADIO_DUPLEX_FULL;
    }

    if (str_ieq(value, "half") || str_ieq(value, "half-duplex") ||
        str_ieq(value, "duplex") || str_ieq(value, "simplex")) {
        return HYBBX_PACKET_RADIO_DUPLEX_HALF;
    }

    return HYBBX_PACKET_RADIO_DUPLEX_UNSET;
}

static hybbx_packet_radio_modulation_t parse_modulation(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_PACKET_RADIO_MOD_UNSET;
    }

    if (str_ieq(value, "g3ruh") || str_ieq(value, "g3ruh_fsk") ||
        str_ieq(value, "fsk9600") || str_ieq(value, "9600fsk")) {
        return HYBBX_PACKET_RADIO_MOD_G3RUH_FSK;
    }

    if (str_ieq(value, "afsk") || str_ieq(value, "bell202") ||
        str_ieq(value, "tcm3105") || str_ieq(value, "1200")) {
        return HYBBX_PACKET_RADIO_MOD_AFSK;
    }

    return HYBBX_PACKET_RADIO_MOD_UNSET;
}

static hybbx_tnc_serial_parity_t parse_serial_parity(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_TNC_SERIAL_PARITY_UNSET;
    }

    if (str_ieq(value, "even") || str_ieq(value, "e")) {
        return HYBBX_TNC_SERIAL_PARITY_EVEN;
    }

    if (str_ieq(value, "odd") || str_ieq(value, "o")) {
        return HYBBX_TNC_SERIAL_PARITY_ODD;
    }

    if (str_ieq(value, "none") || str_ieq(value, "n") || str_ieq(value, "off")) {
        return HYBBX_TNC_SERIAL_PARITY_NONE;
    }

    return HYBBX_TNC_SERIAL_PARITY_UNSET;
}

static void apply_serial_line(hybbx_packet_radio_config_t *out, const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return;
    }

    if (str_ieq(value, "7e1")) {
        out->params.data_bits = 7;
        out->params.serial_parity = HYBBX_TNC_SERIAL_PARITY_EVEN;
        out->params.stop_bits = 1;
        return;
    }

    if (str_ieq(value, "8n1")) {
        out->params.data_bits = 8;
        out->params.serial_parity = HYBBX_TNC_SERIAL_PARITY_NONE;
        out->params.stop_bits = 1;
        return;
    }

    if (str_ieq(value, "7o1")) {
        out->params.data_bits = 7;
        out->params.serial_parity = HYBBX_TNC_SERIAL_PARITY_ODD;
        out->params.stop_bits = 1;
    }
}

static hybbx_kiss_entry_t parse_kiss_entry(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_KISS_ENTRY_UNSET;
    }

    if (str_ieq(value, "none") || str_ieq(value, "skip") ||
        str_ieq(value, "off") || str_ieq(value, "no")) {
        return HYBBX_KISS_ENTRY_NONE;
    }

    if (str_ieq(value, "kiss_on") || str_ieq(value, "kiss-on") ||
        str_ieq(value, "command") || str_ieq(value, "on") ||
        str_ieq(value, "yes")) {
        return HYBBX_KISS_ENTRY_KISS_ON;
    }

    if (str_ieq(value, "esc_at_k") || str_ieq(value, "esc@k") ||
        str_ieq(value, "esc") || str_ieq(value, "@k")) {
        return HYBBX_KISS_ENTRY_ESC_AT_K;
    }

    if (str_ieq(value, "auto")) {
        return HYBBX_KISS_ENTRY_AUTO;
    }

    return HYBBX_KISS_ENTRY_UNSET;
}

static hybbx_kiss_exit_t parse_kiss_exit(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return HYBBX_KISS_EXIT_UNSET;
    }

    if (str_ieq(value, "none") || str_ieq(value, "skip") ||
        str_ieq(value, "off") || str_ieq(value, "no")) {
        return HYBBX_KISS_EXIT_NONE;
    }

    if (str_ieq(value, "kiss_off") || str_ieq(value, "kiss-off") ||
        str_ieq(value, "command")) {
        return HYBBX_KISS_EXIT_KISS_OFF;
    }

    if (str_ieq(value, "kiss_frame") || str_ieq(value, "frame") ||
        str_ieq(value, "fend")) {
        return HYBBX_KISS_EXIT_KISS_FRAME;
    }

    if (str_ieq(value, "auto")) {
        return HYBBX_KISS_EXIT_AUTO;
    }

    return HYBBX_KISS_EXIT_UNSET;
}

static unsigned int default_radio_baud_for_modem(hybbx_tnc2c_modem_t modem)
{
    switch (modem) {
    case HYBBX_TNC2C_MODEM_9600:
        return 9600;
    case HYBBX_TNC2C_MODEM_19200:
        return 19200;
    case HYBBX_TNC2C_MODEM_TCM3105:
    default:
        return HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
    }
}

static hybbx_result_t parse_via_list(hybbx_packet_radio_config_t *out,
                                     const char *value)
{
    char scratch[256];
    char *cursor;
    char *token;

    if (value == NULL || value[0] == '\0') {
        return HYBBX_OK;
    }

    if (strlen(value) >= sizeof(scratch)) {
        return HYBBX_ERR_INVALID;
    }

    memcpy(scratch, value, strlen(value) + 1);
    cursor = scratch;

    while ((token = strtok(cursor, ",")) != NULL && out->via_count < HYBBX_PACKET_RADIO_VIA_MAX) {
        char *item = hybbx_strdup(token);
        if (item == NULL) {
            return HYBBX_ERR_NOMEM;
        }
        out->via[out->via_count++] = item;
        cursor = NULL;
    }

    return HYBBX_OK;
}

int hybbx_packet_radio_baud_valid(unsigned int baud)
{
    return baud > 0;
}

hybbx_result_t hybbx_packet_radio_config_parse(const char *config,
                                               hybbx_packet_radio_config_t *out)
{
    char scratch[256];
    const char *value;
    unsigned long baud;
    hybbx_result_t rc;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));

    out->tnc = HYBBX_PACKET_RADIO_TNC_TNC2C;
    out->protocol = 0; /* profile may choose */
    out->params.clock_mhz = HYBBX_TNC2C_CLOCK_4_9_MHZ;
    out->params.modem = HYBBX_TNC2C_MODEM_TCM3105;
    out->params.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
    out->params.kiss_port = 0;
    out->params.txdelay = 50;
    out->params.persist = 0;
    out->params.slot = 10;
    out->params.txtail = 0;
    out->params.band = HYBBX_PACKET_RADIO_BAND_UNSET;
    out->params.duplex = HYBBX_PACKET_RADIO_DUPLEX_UNSET;
    out->params.modulation = HYBBX_PACKET_RADIO_MOD_UNSET;
    out->params.kiss_entry = HYBBX_KISS_ENTRY_UNSET;
    out->params.kiss_exit = HYBBX_KISS_EXIT_UNSET;
    out->params.host_connect_on_start = 0;
    out->params.assert_modem_lines = -1;

    value = find_kv(config, "device_type", scratch, sizeof(scratch));
    out->device_type = parse_device_type(value);

    value = find_kv(config, "device", scratch, sizeof(scratch));
    if (value == NULL) {
        out->device = hybbx_strdup(default_device_for_type(out->device_type));
    } else {
        out->device = hybbx_strdup(value);
    }

    if (out->device == NULL) {
        hybbx_packet_radio_config_free(out);
        return HYBBX_ERR_NOMEM;
    }

    value = find_kv(config, "baud", scratch, sizeof(scratch));
    if (value == NULL || value[0] == '\0') {
        out->baud = HYBBX_PACKET_RADIO_DEFAULT_BAUD;
    } else {
        baud = strtoul(value, NULL, 10);
        if (!hybbx_packet_radio_baud_valid((unsigned int)baud)) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_INVALID;
        }
        out->baud = (unsigned int)baud;
    }

    value = find_kv(config, "tnc", scratch, sizeof(scratch));
    out->tnc = parse_tnc(value);

    value = find_kv(config, "protocol", scratch, sizeof(scratch));
    if (value != NULL) {
        out->protocol = parse_protocol(value);
    }

    value = find_kv(config, "clock_mhz", scratch, sizeof(scratch));
    out->params.clock_mhz = parse_clock_mhz(value);

    value = find_kv(config, "modem", scratch, sizeof(scratch));
    out->params.modem = parse_modem(value);

    value = find_kv(config, "radio_baud", scratch, sizeof(scratch));
    if (value == NULL) {
        out->params.radio_baud = default_radio_baud_for_modem(out->params.modem);
    } else {
        unsigned int radio_baud = parse_uint(value, 0);
        if (!hybbx_packet_radio_baud_valid(radio_baud)) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_INVALID;
        }
        out->params.radio_baud = radio_baud;
    }

    value = find_kv(config, "kiss_port", scratch, sizeof(scratch));
    out->params.kiss_port = parse_uint(value, 0);

    value = find_kv(config, "txdelay", scratch, sizeof(scratch));
    out->params.txdelay = parse_uint(value, out->params.txdelay);

    value = find_kv(config, "persist", scratch, sizeof(scratch));
    out->params.persist = parse_uint(value, out->params.persist);

    value = find_kv(config, "slot", scratch, sizeof(scratch));
    out->params.slot = parse_uint(value, out->params.slot);

    value = find_kv(config, "txtail", scratch, sizeof(scratch));
    out->params.txtail = parse_uint(value, out->params.txtail);

    value = find_kv(config, "radio_band", scratch, sizeof(scratch));
    if (value != NULL) {
        out->params.band = parse_radio_band(value);
    }

    value = find_kv(config, "radio_duplex", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "duplex", scratch, sizeof(scratch));
    }
    if (value != NULL) {
        out->params.duplex = parse_radio_duplex(value);
    }

    value = find_kv(config, "fullduplex", scratch, sizeof(scratch));
    if (value != NULL &&
        out->params.duplex == HYBBX_PACKET_RADIO_DUPLEX_UNSET) {
        out->params.duplex = hybbx_parse_bool(value, 0) ?
            HYBBX_PACKET_RADIO_DUPLEX_FULL :
            HYBBX_PACKET_RADIO_DUPLEX_HALF;
    }

    value = find_kv(config, "g3ruh_fsk", scratch, sizeof(scratch));
    if (value != NULL) {
        out->params.modulation = hybbx_parse_bool(value, 0) ?
            HYBBX_PACKET_RADIO_MOD_G3RUH_FSK :
            HYBBX_PACKET_RADIO_MOD_AFSK;
    }

    value = find_kv(config, "modulation", scratch, sizeof(scratch));
    if (value != NULL) {
        hybbx_packet_radio_modulation_t mod = parse_modulation(value);
        if (mod != HYBBX_PACKET_RADIO_MOD_UNSET) {
            out->params.modulation = mod;
        }
    }

    value = find_kv(config, "kiss_entry", scratch, sizeof(scratch));
    if (value != NULL) {
        hybbx_kiss_entry_t kiss_entry = parse_kiss_entry(value);
        if (kiss_entry != HYBBX_KISS_ENTRY_UNSET) {
            out->params.kiss_entry = kiss_entry;
        }
    }

    value = find_kv(config, "kiss_exit", scratch, sizeof(scratch));
    if (value != NULL) {
        hybbx_kiss_exit_t kiss_exit = parse_kiss_exit(value);
        if (kiss_exit != HYBBX_KISS_EXIT_UNSET) {
            out->params.kiss_exit = kiss_exit;
        }
    }

    value = find_kv(config, "host_connect", scratch, sizeof(scratch));
    out->params.host_connect_on_start = hybbx_parse_bool(value,
                                                   out->params.host_connect_on_start);

    value = find_kv(config, "serial_line", scratch, sizeof(scratch));
    if (value != NULL) {
        apply_serial_line(out, value);
    }

    value = find_kv(config, "data_bits", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        unsigned int data_bits = parse_uint(value, 0);
        if (data_bits == 7 || data_bits == 8) {
            out->params.data_bits = data_bits;
        }
    }

    value = find_kv(config, "parity", scratch, sizeof(scratch));
    if (value != NULL) {
        hybbx_tnc_serial_parity_t parity = parse_serial_parity(value);
        if (parity != HYBBX_TNC_SERIAL_PARITY_UNSET) {
            out->params.serial_parity = parity;
        }
    }

    value = find_kv(config, "stop_bits", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        unsigned int stop_bits = parse_uint(value, 0);
        if (stop_bits == 1 || stop_bits == 2) {
            out->params.stop_bits = stop_bits;
        }
    }

    value = find_kv(config, "assert_modem_lines", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "rts_dtr", scratch, sizeof(scratch));
    }
    if (value != NULL) {
        out->params.assert_modem_lines = hybbx_parse_bool(value, 0) ? 1 : 0;
    }

    value = find_kv(config, "mycall", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->mycall = hybbx_strdup(value);
        if (out->mycall == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "dest", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "dest_call", scratch, sizeof(scratch));
    }
    if (value != NULL && value[0] != '\0') {
        out->dest_call = hybbx_strdup(value);
        if (out->dest_call == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "via", scratch, sizeof(scratch));
    rc = parse_via_list(out, value);
    if (rc != HYBBX_OK) {
        hybbx_packet_radio_config_free(out);
        return rc;
    }

    value = find_kv(config, "circuit_host", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->circuit_host = hybbx_strdup(value);
        if (out->circuit_host == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "circuit_port", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->circuit_port = parse_uint(value, HYBBX_CIRCUIT_DEFAULT_PORT);
    }

    if (out->circuit_port == 0) {
        out->circuit_port = HYBBX_CIRCUIT_DEFAULT_PORT;
    }
    if (out->circuit_host == NULL) {
        out->circuit_host = hybbx_strdup("127.0.0.1");
        if (out->circuit_host == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "link_id", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->link_id = hybbx_strdup(value);
        if (out->link_id == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "link_password", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->link_password = hybbx_strdup(value);
        if (out->link_password == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "link_role", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->link_role = hybbx_strdup(value);
        if (out->link_role == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    value = find_kv(config, "frequency_mhz", scratch, sizeof(scratch));
    if (value == NULL || value[0] == '\0') {
        value = find_kv(config, "frequency", scratch, sizeof(scratch));
    }
    if (value != NULL && value[0] != '\0') {
        out->frequency_mhz = hybbx_strdup(value);
        if (out->frequency_mhz == NULL) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_NOMEM;
        }
    }

    (void)hybbx_tnc_profile_apply_defaults(out);
    return HYBBX_OK;
}

void hybbx_packet_radio_config_free(hybbx_packet_radio_config_t *config)
{
    unsigned i;

    if (config == NULL) {
        return;
    }

    free(config->device);
    free(config->mycall);
    free(config->dest_call);
    free(config->circuit_host);
    free(config->link_id);
    free(config->link_password);
    free(config->link_role);
    free(config->frequency_mhz);
    for (i = 0; i < config->via_count; i++) {
        free(config->via[i]);
        config->via[i] = NULL;
    }

    memset(config, 0, sizeof(*config));
}
