#include "hybbx/registry.h"
#include "hybbx/limits.h"

#include <string.h>

typedef struct registry_entry {
    const hybbx_transport_plugin_t *plugin;
} registry_entry_t;

static registry_entry_t g_registry[HYBBX_MAX_PLUGINS];
static size_t g_registry_count = 0;

hybbx_result_t hybbx_registry_register(const hybbx_transport_plugin_t *plugin)
{
    if (plugin == NULL || plugin->name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_registry_find(plugin->name) != NULL) {
        return HYBBX_ERR_BUSY;
    }

    if (g_registry_count >= HYBBX_MAX_PLUGINS) {
        return HYBBX_ERR_NOMEM;
    }

    g_registry[g_registry_count].plugin = plugin;
    g_registry_count++;
    return HYBBX_OK;
}

hybbx_result_t hybbx_registry_unregister(const char *name)
{
    size_t i;

    if (name == NULL) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].plugin->name, name) == 0) {
            size_t remaining = g_registry_count - i - 1;
            if (remaining > 0) {
                memmove(&g_registry[i], &g_registry[i + 1],
                        remaining * sizeof(registry_entry_t));
            }
            g_registry_count--;
            return HYBBX_OK;
        }
    }

    return HYBBX_ERR_NOT_FOUND;
}

const hybbx_transport_plugin_t *hybbx_registry_find(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].plugin->name, name) == 0) {
            return g_registry[i].plugin;
        }
    }

    return NULL;
}

void hybbx_registry_foreach(hybbx_registry_iter_fn fn, void *userdata)
{
    size_t i;

    if (fn == NULL) {
        return;
    }

    for (i = 0; i < g_registry_count; i++) {
        fn(g_registry[i].plugin, userdata);
    }
}
