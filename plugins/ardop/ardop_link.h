#ifndef ARDOP_LINK_H
#define ARDOP_LINK_H

#include "hybbx/plugin.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;

typedef hybbx_result_t (*ardop_link_config_fn)(const char *config,
                                               void *config_out);

typedef struct ardop_link_plugin {
    const char *name;
    hybbx_transport_kind_t kind;
    const char *default_link_id;
    const char *modem_hint;
    ardop_link_config_fn parse_config;
    void (*config_free)(void *config_out);
    size_t config_size;
} ardop_link_plugin_t;

hybbx_result_t ardop_link_init(struct hybbx_service *service,
                               const ardop_link_plugin_t *plugin);
void ardop_link_shutdown(const ardop_link_plugin_t *plugin);
hybbx_result_t ardop_link_start(const ardop_link_plugin_t *plugin,
                                const char *config);
hybbx_result_t ardop_link_stop(const ardop_link_plugin_t *plugin);

#ifdef __cplusplus
}
#endif

#endif /* ARDOP_LINK_H */
