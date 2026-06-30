#include "hybbx/packet_radio.h"
#include "hybbx/tnc2c.h"

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

    if (str_ieq(value, "generic")) {
        return HYBBX_PACKET_RADIO_TNC_GENERIC;
    }

    return HYBBX_PACKET_RADIO_TNC_TNC2C;
}

static hybbx_packet_radio_protocol_t parse_protocol(const char *value)
{
    if (value == NULL || str_ieq(value, "kiss")) {
        return HYBBX_PACKET_RADIO_PROTO_KISS;
    }

    if (str_ieq(value, "hostmode") || str_ieq(value, "host")) {
        return HYBBX_PACKET_RADIO_PROTO_HOSTMODE;
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

static int parse_bool(const char *value, int default_value)
{
    if (value == NULL) {
        return default_value;
    }

    if (str_ieq(value, "yes") || str_ieq(value, "true") || strcmp(value, "1") == 0) {
        return 1;
    }

    if (str_ieq(value, "no") || str_ieq(value, "false") || strcmp(value, "0") == 0) {
        return 0;
    }

    return default_value;
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

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));

    out->tnc = HYBBX_PACKET_RADIO_TNC_TNC2C;
    out->protocol = HYBBX_PACKET_RADIO_PROTO_KISS;
    out->tnc2c.clock_mhz = HYBBX_TNC2C_CLOCK_4_9_MHZ;
    out->tnc2c.modem = HYBBX_TNC2C_MODEM_TCM3105;
    out->tnc2c.radio_baud = HYBBX_TNC2C_DEFAULT_RADIO_BAUD;
    out->tnc2c.kiss_port = 0;
    out->tnc2c.txdelay = 50;
    out->tnc2c.persist = 128;
    out->tnc2c.slot = 10;
    out->tnc2c.txtail = 0;
    out->tnc2c.fullduplex = 0;
    out->tnc2c.kiss_on_startup = 1;

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
    out->protocol = parse_protocol(value);

    value = find_kv(config, "clock_mhz", scratch, sizeof(scratch));
    out->tnc2c.clock_mhz = parse_clock_mhz(value);

    value = find_kv(config, "modem", scratch, sizeof(scratch));
    out->tnc2c.modem = parse_modem(value);

    value = find_kv(config, "radio_baud", scratch, sizeof(scratch));
    if (value == NULL) {
        out->tnc2c.radio_baud = default_radio_baud_for_modem(out->tnc2c.modem);
    } else {
        unsigned int radio_baud = parse_uint(value, 0);
        if (!hybbx_packet_radio_baud_valid(radio_baud)) {
            hybbx_packet_radio_config_free(out);
            return HYBBX_ERR_INVALID;
        }
        out->tnc2c.radio_baud = radio_baud;
    }

    value = find_kv(config, "kiss_port", scratch, sizeof(scratch));
    out->tnc2c.kiss_port = parse_uint(value, 0);

    value = find_kv(config, "txdelay", scratch, sizeof(scratch));
    out->tnc2c.txdelay = parse_uint(value, out->tnc2c.txdelay);

    value = find_kv(config, "persist", scratch, sizeof(scratch));
    out->tnc2c.persist = parse_uint(value, out->tnc2c.persist);

    value = find_kv(config, "slot", scratch, sizeof(scratch));
    out->tnc2c.slot = parse_uint(value, out->tnc2c.slot);

    value = find_kv(config, "txtail", scratch, sizeof(scratch));
    out->tnc2c.txtail = parse_uint(value, out->tnc2c.txtail);

    value = find_kv(config, "fullduplex", scratch, sizeof(scratch));
    out->tnc2c.fullduplex = parse_bool(value, out->tnc2c.fullduplex);

    value = find_kv(config, "kiss_on_startup", scratch, sizeof(scratch));
    out->tnc2c.kiss_on_startup = parse_bool(value, out->tnc2c.kiss_on_startup);

    return HYBBX_OK;
}

void hybbx_packet_radio_config_free(hybbx_packet_radio_config_t *config)
{
    if (config == NULL) {
        return;
    }

    free(config->device);
    config->device = NULL;
    config->baud = 0;
}
