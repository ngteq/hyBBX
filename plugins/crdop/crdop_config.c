#include "hybbx/ardop.h"
#include "hybbx/crdop.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *crdop_strdup(const char *s)
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

static const char *find_host(const char *config, char *scratch, size_t len)
{
    const char *value;

    value = find_kv(config, "modem_host", scratch, len);
    if (value != NULL && value[0] != '\0') {
        return value;
    }
    return find_kv(config, "ardop_host", scratch, len);
}

static const char *find_port_str(const char *config, char *scratch, size_t len)
{
    const char *value;

    value = find_kv(config, "modem_port", scratch, len);
    if (value != NULL && value[0] != '\0') {
        return value;
    }
    return find_kv(config, "ardop_port", scratch, len);
}

hybbx_result_t hybbx_crdop_config_parse(const char *config,
                                        hybbx_ardop_config_t *out)
{
    char scratch[128];
    const char *value;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_ardop_config_free(out);

    if (config == NULL) {
        config = "";
    }

    value = find_host(config, scratch, sizeof(scratch));
    out->ardop_host = crdop_strdup(value != NULL ? value : "127.0.0.1");

    out->ardop_port = (unsigned)strtoul(
        find_port_str(config, scratch, sizeof(scratch)) != NULL ?
        scratch : "8515", NULL, 10);

    value = find_kv(config, "mycall", scratch, sizeof(scratch));
    out->mycall = crdop_strdup(value != NULL ? value : "NOCALL");

    out->radio_profile = HYBBX_CRDOP_PROFILE_CB;

    value = find_kv(config, "arq_bandwidth", scratch, sizeof(scratch));
    out->arq_bandwidth = crdop_strdup(
        value != NULL ? value :
        hybbx_crdop_default_arq_bandwidth(HYBBX_CRDOP_PROFILE_CB));

    value = find_kv(config, "peer_call", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->peer_call = crdop_strdup(value);
    }

    value = find_kv(config, "listen", scratch, sizeof(scratch));
    out->listen = value == NULL || str_ieq(value, "yes") || str_ieq(value, "1") ||
                  str_ieq(value, "true");

    value = find_kv(config, "circuit_host", scratch, sizeof(scratch));
    out->circuit_host = crdop_strdup(value != NULL ? value : "127.0.0.1");

    out->circuit_port = (unsigned)strtoul(
        find_kv(config, "circuit_port", scratch, sizeof(scratch)) != NULL ?
        scratch : "7323", NULL, 10);

    value = find_kv(config, "link_id", scratch, sizeof(scratch));
    out->link_id = crdop_strdup(value != NULL ? value : "crdop-link");

    value = find_kv(config, "link_password", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->link_password = crdop_strdup(value);
    }

    value = find_kv(config, "link_role", scratch, sizeof(scratch));
    out->link_role = crdop_strdup(value != NULL ? value : "link");

    value = find_kv(config, "frequency_mhz", scratch, sizeof(scratch));
    if (value == NULL || value[0] == '\0') {
        value = find_kv(config, "frequency", scratch, sizeof(scratch));
    }
    if (value != NULL && value[0] != '\0') {
        out->frequency_mhz = strtod(value, NULL);
    }

    if (hybbx_crdop_bandwidth_exceeds_cb(out->arq_bandwidth)) {
        hybbx_log_warn("[crdop] warning: arq_bandwidth=%s high for CB profile",
                       out->arq_bandwidth);
    }

    if (out->ardop_host == NULL || out->mycall == NULL ||
        out->arq_bandwidth == NULL || out->circuit_host == NULL ||
        out->link_id == NULL || out->link_role == NULL) {
        hybbx_ardop_config_free(out);
        return HYBBX_ERR_NOMEM;
    }

    return HYBBX_OK;
}
