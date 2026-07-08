#include "hybbx/baycom.h"
#include "hybbx/circuit.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *baycom_strdup(const char *s)
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

const char *hybbx_baycom_backend_name(hybbx_baycom_backend_t backend)
{
    switch (backend) {
    case HYBBX_BAYCOM_BACKEND_KISS:
        return "kiss";
    case HYBBX_BAYCOM_BACKEND_KERNEL:
    default:
        return "kernel";
    }
}

const char *hybbx_baycom_modem_mode_name(hybbx_baycom_modem_mode_t mode)
{
    switch (mode) {
    case HYBBX_BAYCOM_MODE_SER12_SOFTDCD:
        return "ser12*";
    case HYBBX_BAYCOM_MODE_SER12_INV:
        return "ser12+";
    case HYBBX_BAYCOM_MODE_SER12HDX:
        return "ser12hdx";
    case HYBBX_BAYCOM_MODE_PAR96:
        return "par96";
    case HYBBX_BAYCOM_MODE_PAR96_SOFTDCD:
        return "par96*";
    case HYBBX_BAYCOM_MODE_EPP:
        return "epp";
    case HYBBX_BAYCOM_MODE_SER12:
    default:
        return "ser12";
    }
}

static hybbx_baycom_backend_t parse_backend(const char *value)
{
    if (value == NULL || str_ieq(value, "kernel") || str_ieq(value, "hdlc") ||
        str_ieq(value, "prstack") || str_ieq(value, "l2")) {
        return HYBBX_BAYCOM_BACKEND_KERNEL;
    }

    if (str_ieq(value, "kiss") || str_ieq(value, "serial")) {
        return HYBBX_BAYCOM_BACKEND_KISS;
    }

    return HYBBX_BAYCOM_BACKEND_KERNEL;
}

static hybbx_baycom_modem_mode_t parse_mode(const char *value)
{
    if (value == NULL || str_ieq(value, "ser12")) {
        return HYBBX_BAYCOM_MODE_SER12;
    }
    if (str_ieq(value, "ser12*") || str_ieq(value, "ser12_softdcd")) {
        return HYBBX_BAYCOM_MODE_SER12_SOFTDCD;
    }
    if (str_ieq(value, "ser12+") || str_ieq(value, "ser12_inv")) {
        return HYBBX_BAYCOM_MODE_SER12_INV;
    }
    if (str_ieq(value, "ser12hdx") || str_ieq(value, "ser12_hdx") ||
        str_ieq(value, "hdx")) {
        return HYBBX_BAYCOM_MODE_SER12HDX;
    }
    if (str_ieq(value, "par96")) {
        return HYBBX_BAYCOM_MODE_PAR96;
    }
    if (str_ieq(value, "par96*") || str_ieq(value, "par96_softdcd")) {
        return HYBBX_BAYCOM_MODE_PAR96_SOFTDCD;
    }
    if (str_ieq(value, "epp") || str_ieq(value, "par97") ||
        str_ieq(value, "baycom_epp")) {
        return HYBBX_BAYCOM_MODE_EPP;
    }

    return HYBBX_BAYCOM_MODE_SER12_SOFTDCD;
}

void hybbx_baycom_config_free(hybbx_baycom_config_t *config)
{
    if (config == NULL) {
        return;
    }

    free(config->interface);
    free(config->kernel_module);
    free(config->device);
    free(config->mycall);
    free(config->circuit_host);
    free(config->link_id);
    free(config->link_password);
    free(config->link_role);
    free(config->frequency_mhz);
    memset(config, 0, sizeof(*config));
}

static void apply_mode_defaults(hybbx_baycom_config_t *out)
{
    if (out->interface == NULL) {
        switch (out->mode) {
        case HYBBX_BAYCOM_MODE_SER12HDX:
            out->interface = baycom_strdup("bcsh0");
            break;
        case HYBBX_BAYCOM_MODE_PAR96:
        case HYBBX_BAYCOM_MODE_PAR96_SOFTDCD:
            out->interface = baycom_strdup("bcp0");
            break;
        case HYBBX_BAYCOM_MODE_EPP:
            out->interface = baycom_strdup("bce0");
            break;
        case HYBBX_BAYCOM_MODE_SER12:
        case HYBBX_BAYCOM_MODE_SER12_SOFTDCD:
        case HYBBX_BAYCOM_MODE_SER12_INV:
        default:
            out->interface = baycom_strdup(HYBBX_BAYCOM_DEFAULT_INTERFACE);
            break;
        }
    }

    if (out->kernel_module == NULL) {
        switch (out->mode) {
        case HYBBX_BAYCOM_MODE_SER12HDX:
            out->kernel_module = baycom_strdup("baycom_ser_hdx");
            break;
        case HYBBX_BAYCOM_MODE_PAR96:
        case HYBBX_BAYCOM_MODE_PAR96_SOFTDCD:
            out->kernel_module = baycom_strdup("baycom_par");
            break;
        case HYBBX_BAYCOM_MODE_EPP:
            out->kernel_module = baycom_strdup("baycom_epp");
            break;
        default:
            out->kernel_module = baycom_strdup("baycom_ser_fdx");
            break;
        }
    }

    if (out->device == NULL) {
        out->device = baycom_strdup("/dev/ttyS0");
    }
}

hybbx_result_t hybbx_baycom_config_parse(const char *config,
                                         hybbx_baycom_config_t *out)
{
    char scratch[128];
    const char *value;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_baycom_config_free(out);

    if (config == NULL) {
        config = "";
    }

    value = find_kv(config, "backend", scratch, sizeof(scratch));
    out->backend = parse_backend(value);

    value = find_kv(config, "mode", scratch, sizeof(scratch));
    out->mode = parse_mode(value);

    value = find_kv(config, "interface", scratch, sizeof(scratch));
    if (value != NULL) {
        out->interface = baycom_strdup(value);
    }

    value = find_kv(config, "kernel_module", scratch, sizeof(scratch));
    if (value != NULL) {
        out->kernel_module = baycom_strdup(value);
    }

    value = find_kv(config, "iobase", scratch, sizeof(scratch));
    out->iobase = value != NULL ? (unsigned)strtoul(value, NULL, 0)
                                : HYBBX_BAYCOM_DEFAULT_IOBASE;

    value = find_kv(config, "irq", scratch, sizeof(scratch));
    out->irq = value != NULL ? (unsigned)strtoul(value, NULL, 0)
                             : HYBBX_BAYCOM_DEFAULT_IRQ;

    value = find_kv(config, "radio_baud", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "baud", scratch, sizeof(scratch));
    }
    out->radio_baud = value != NULL ? (unsigned)strtoul(value, NULL, 0)
                                    : HYBBX_BAYCOM_DEFAULT_RADIO_BAUD;

    value = find_kv(config, "kernel_autoload", scratch, sizeof(scratch));
    out->kernel_autoload = value != NULL ? hybbx_parse_bool(value, 0) : 0;

    value = find_kv(config, "txdelay", scratch, sizeof(scratch));
    out->txdelay = value != NULL ? (unsigned)strtoul(value, NULL, 0) : 30u;

    value = find_kv(config, "txtail", scratch, sizeof(scratch));
    out->txtail = value != NULL ? (unsigned)strtoul(value, NULL, 0) : 1u;

    value = find_kv(config, "slot", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "slottime", scratch, sizeof(scratch));
    }
    out->slot = value != NULL ? (unsigned)strtoul(value, NULL, 0) : 10u;

    value = find_kv(config, "persist", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "ppersist", scratch, sizeof(scratch));
    }
    out->persist = value != NULL ? (unsigned)strtoul(value, NULL, 0) : 128u;

    value = find_kv(config, "full_duplex", scratch, sizeof(scratch));
    if (value != NULL) {
        out->full_duplex = hybbx_parse_bool(value, 0);
    } else {
        value = find_kv(config, "duplex", scratch, sizeof(scratch));
        out->full_duplex = value != NULL && str_ieq(value, "full");
    }

    value = find_kv(config, "device", scratch, sizeof(scratch));
    if (value != NULL) {
        out->device = baycom_strdup(value);
    }

    value = find_kv(config, "serial_baud", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "host_baud", scratch, sizeof(scratch));
    }
    out->serial_baud = value != NULL ? (unsigned)strtoul(value, NULL, 0) : 1200u;

    value = find_kv(config, "mycall", scratch, sizeof(scratch));
    if (value != NULL) {
        out->mycall = baycom_strdup(value);
    }

    value = find_kv(config, "circuit_host", scratch, sizeof(scratch));
    out->circuit_host = baycom_strdup(value != NULL ? value : "127.0.0.1");

    value = find_kv(config, "circuit_port", scratch, sizeof(scratch));
    out->circuit_port = value != NULL ? (unsigned)strtoul(value, NULL, 0)
                                      : HYBBX_CIRCUIT_DEFAULT_PORT;

    value = find_kv(config, "link_id", scratch, sizeof(scratch));
    out->link_id = baycom_strdup(value != NULL ? value : "baycom-link");

    value = find_kv(config, "link_password", scratch, sizeof(scratch));
    out->link_password = baycom_strdup(value != NULL ? value : "");

    value = find_kv(config, "link_role", scratch, sizeof(scratch));
    out->link_role = baycom_strdup(value != NULL ? value : "link");

    value = find_kv(config, "frequency_mhz", scratch, sizeof(scratch));
    if (value != NULL) {
        out->frequency_mhz = baycom_strdup(value);
    }

    apply_mode_defaults(out);

    if (out->circuit_host == NULL || out->link_id == NULL) {
        hybbx_baycom_config_free(out);
        return HYBBX_ERR_NOMEM;
    }

    return HYBBX_OK;
}
