#ifndef HYBBX_TNC_HOST_H
#define HYBBX_TNC_HOST_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hybbx_tnc_host_state {
    HYBBX_TNC_HOST_BOOT = 0,
    HYBBX_TNC_HOST_CMD = 1,
    HYBBX_TNC_HOST_CONNECTED = 2
} hybbx_tnc_host_state_t;

typedef void (*hybbx_tnc_host_text_cb)(const uint8_t *data, size_t len,
                                         void *userdata);

typedef void (*hybbx_tnc_host_event_cb)(hybbx_tnc_host_state_t state,
                                        void *userdata);

typedef struct hybbx_tnc_host {
    hybbx_tnc_host_state_t state;
    char line[256];
    size_t line_len;
    hybbx_tnc_host_text_cb text_cb;
    hybbx_tnc_host_event_cb event_cb;
    void *userdata;
} hybbx_tnc_host_t;

void hybbx_tnc_host_init(hybbx_tnc_host_t *host,
                         hybbx_tnc_host_text_cb text_cb,
                         hybbx_tnc_host_event_cb event_cb,
                         void *userdata);

void hybbx_tnc_host_reset(hybbx_tnc_host_t *host);

void hybbx_tnc_host_feed(hybbx_tnc_host_t *host,
                         const uint8_t *data, size_t len);

/** Build a TNC2 command line (adds CR). Returns length or 0. */
size_t hybbx_tnc_host_format_cmd(const char *cmd, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TNC_HOST_H */
