#ifndef HYBBX_REGISTRY_H
#define HYBBX_REGISTRY_H

#include "hybbx/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Register a transport plugin (called at plugin init time). */
hybbx_result_t hybbx_registry_register(const hybbx_transport_plugin_t *plugin);

/** Unregister a transport plugin by name. */
hybbx_result_t hybbx_registry_unregister(const char *name);

/** Look up a registered plugin by name. Returns NULL if not found. */
const hybbx_transport_plugin_t *hybbx_registry_find(const char *name);

/** Iterate over all registered plugins. */
typedef void (*hybbx_registry_iter_fn)(const hybbx_transport_plugin_t *plugin,
                                       void *userdata);
void hybbx_registry_foreach(hybbx_registry_iter_fn fn, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_REGISTRY_H */
