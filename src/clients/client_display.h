#ifndef HYBBX_CLIENT_DISPLAY_H
#define HYBBX_CLIENT_DISPLAY_H

#include "hybbx/traffic.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_client_display {
    const hybbx_traffic_config_t *traffic;
    unsigned col;
} hybbx_client_display_t;

void hybbx_client_display_init(hybbx_client_display_t *disp,
                               const hybbx_traffic_config_t *traffic);

void hybbx_client_display_byte(hybbx_client_display_t *disp, uint8_t byte);

void hybbx_client_display_write(hybbx_client_display_t *disp,
                                const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CLIENT_DISPLAY_H */
