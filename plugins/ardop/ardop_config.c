#include "hybbx/ardop.h"
#include "hybbx/crdop.h"
#include "hybbx/circuit.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ardop_strdup(const char *s)
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

void hybbx_ardop_config_free(hybbx_ardop_config_t *config)
{
    if (config == NULL) {
        return;
    }

    free(config->ardop_host);
    free(config->mycall);
    free(config->arq_bandwidth);
    free(config->peer_call);
    free(config->circuit_host);
    free(config->link_id);
    free(config->link_password);
    free(config->link_role);
    memset(config, 0, sizeof(*config));
}

hybbx_result_t hybbx_ardop_config_parse(const char *config,
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

    value = find_kv(config, "ardop_host", scratch, sizeof(scratch));
    out->ardop_host = ardop_strdup(value != NULL ? value : "127.0.0.1");

    out->ardop_port = (unsigned)strtoul(
        find_kv(config, "ardop_port", scratch, sizeof(scratch)) != NULL ?
        scratch : "8515", NULL, 10);

    value = find_kv(config, "mycall", scratch, sizeof(scratch));
    out->mycall = ardop_strdup(value != NULL ? value : "NOCALL");

    value = find_kv(config, "radio_profile", scratch, sizeof(scratch));
    out->radio_profile = hybbx_crdop_profile_parse(value);

    value = find_kv(config, "arq_bandwidth", scratch, sizeof(scratch));
    out->arq_bandwidth = ardop_strdup(
        value != NULL ? value :
        hybbx_crdop_default_arq_bandwidth(out->radio_profile));

    value = find_kv(config, "peer_call", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->peer_call = ardop_strdup(value);
    }

    value = find_kv(config, "listen", scratch, sizeof(scratch));
    out->listen = value == NULL || str_ieq(value, "yes") || str_ieq(value, "1") ||
                  str_ieq(value, "true");

    value = find_kv(config, "circuit_host", scratch, sizeof(scratch));
    out->circuit_host = ardop_strdup(value != NULL ? value : "127.0.0.1");

    out->circuit_port = (unsigned)strtoul(
        find_kv(config, "circuit_port", scratch, sizeof(scratch)) != NULL ?
        scratch : "7323", NULL, 10);

    value = find_kv(config, "link_id", scratch, sizeof(scratch));
    out->link_id = ardop_strdup(value != NULL ? value : "ardop-link");

    value = find_kv(config, "link_password", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        out->link_password = ardop_strdup(value);
    }

    value = find_kv(config, "link_role", scratch, sizeof(scratch));
    out->link_role = ardop_strdup(value != NULL ? value : "link");

    value = find_kv(config, "frequency_mhz", scratch, sizeof(scratch));
    if (value == NULL || value[0] == '\0') {
        value = find_kv(config, "frequency", scratch, sizeof(scratch));
    }
    if (value != NULL && value[0] != '\0') {
        out->frequency_mhz = strtod(value, NULL);
    }

    if (out->radio_profile == HYBBX_CRDOP_PROFILE_CB &&
        hybbx_crdop_bandwidth_exceeds_cb(out->arq_bandwidth)) {
        fprintf(stderr,
                "[ardop] warning: arq_bandwidth=%s high for CB profile "
                "(CRDOP Level 2 experimental)\n",
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
