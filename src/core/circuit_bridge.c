#include "hybbx/circuit_bridge.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bridge_load_ctx {
    hybbx_circuit_bridge_registry_t *reg;
    const hybbx_config_t *reg_config;
} bridge_load_ctx_t;

static int bridge_section_is_numbered_transport(const char *section,
                                               const char *plugin)
{
    char prefix[64];
    size_t prefix_len;
    const char *suffix;

    if (section == NULL || plugin == NULL) {
        return 0;
    }

    snprintf(prefix, sizeof(prefix), "transport.%s", plugin);
    prefix_len = strlen(prefix);
    if (strncmp(section, prefix, prefix_len) != 0) {
        return 0;
    }

    suffix = section + prefix_len;
    if (suffix[0] == '\0') {
        return 0;
    }

    while (*suffix != '\0') {
        if (*suffix < '0' || *suffix > '9') {
            return 0;
        }
        suffix++;
    }

    return 1;
}

static int bridge_section_is_numbered_packet_radio(const char *section)
{
    return bridge_section_is_numbered_transport(section, "packet_radio");
}

static int bridge_section_is_numbered_ardop(const char *section)
{
    return bridge_section_is_numbered_transport(section, "ardop");
}

static int bridge_section_is_numbered_crdop(const char *section)
{
    return bridge_section_is_numbered_transport(section, "crdop");
}

static int bridge_section_is_numbered_baycom(const char *section)
{
    return bridge_section_is_numbered_transport(section, "baycom");
}

static void bridge_load_section(const char *section, void *userdata)
{
    bridge_load_ctx_t *ctx = (bridge_load_ctx_t *)userdata;
    hybbx_circuit_bridge_entry_t *entry;
    const char *value;
    unsigned i;

    if (ctx == NULL || ctx->reg == NULL || section == NULL) {
        return;
    }

    if (!bridge_section_is_numbered_packet_radio(section) &&
        !bridge_section_is_numbered_ardop(section) &&
        !bridge_section_is_numbered_crdop(section) &&
        !bridge_section_is_numbered_baycom(section) &&
        !bridge_section_is_numbered_transport(section, "mains_proxy")) {
        return;
    }

    if (ctx->reg->count >= HYBBX_CIRCUIT_MAX_LINKS) {
        return;
    }

    entry = &ctx->reg->entries[ctx->reg->count];
    memset(entry, 0, sizeof(*entry));

    value = hybbx_config_get(ctx->reg_config, section, "link_id", NULL);
    if (value == NULL || value[0] == '\0') {
        return;
    }
    hybbx_strlcpy(entry->link_id, value, sizeof(entry->link_id));

    for (i = 0; i < ctx->reg->count; i++) {
        if (strcmp(ctx->reg->entries[i].link_id, entry->link_id) == 0) {
            fprintf(stderr,
                    "[circuit] bridge: duplicate link_id=%s in [%s] — skipped\n",
                    entry->link_id, section);
            return;
        }
    }

    value = hybbx_config_get(ctx->reg_config, section, "link_password", NULL);
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(entry->link_password, value, sizeof(entry->link_password));
    }

    value = hybbx_config_get(ctx->reg_config, section, "link_role", NULL);
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(entry->link_role, value, sizeof(entry->link_role));
    } else if (bridge_section_is_numbered_transport(section, "mains_proxy")) {
        hybbx_strlcpy(entry->link_role, "proxy", sizeof(entry->link_role));
    } else {
        hybbx_strlcpy(entry->link_role, "link", sizeof(entry->link_role));
    }

    value = hybbx_config_get(ctx->reg_config, section, "frequency_mhz", NULL);
    if (value == NULL || value[0] == '\0') {
        value = hybbx_config_get(ctx->reg_config, section, "frequency", NULL);
    }
    if (value != NULL && value[0] != '\0') {
        double mhz = strtod(value, NULL);

        if (mhz > 0.0) {
            entry->frequency_mhz = mhz;
        }
    }

    ctx->reg->count++;
}

void hybbx_circuit_bridge_clear(hybbx_circuit_bridge_registry_t *reg)
{
    if (reg == NULL) {
        return;
    }
    memset(reg, 0, sizeof(*reg));
}

hybbx_result_t hybbx_circuit_bridge_load(hybbx_circuit_bridge_registry_t *reg,
                                         const hybbx_config_t *config)
{
    bridge_load_ctx_t ctx;

    if (reg == NULL || config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_circuit_bridge_clear(reg);
    ctx.reg = reg;
    ctx.reg_config = config;
    hybbx_config_foreach_section(config, bridge_load_section, &ctx);
    return HYBBX_OK;
}

const hybbx_circuit_bridge_entry_t *hybbx_circuit_bridge_find(
    const hybbx_circuit_bridge_registry_t *reg, const char *link_id)
{
    unsigned i;

    if (reg == NULL || link_id == NULL || link_id[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].link_id, link_id) == 0) {
            return &reg->entries[i];
        }
    }

    return NULL;
}
