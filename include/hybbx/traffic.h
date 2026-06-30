#ifndef HYBBX_TRAFFIC_H
#define HYBBX_TRAFFIC_H

#include "hybbx/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_config;
struct hybbx_session;

/** Host link speed HyBBX is optimized for (8N1, ~1 byte per 10 bit times). */
#define HYBBX_BAUD2400 2400u

/** Classic packet-BBS display width (columns). */
#define HYBBX_LINE_WIDTH 40u

/** Maximum user input line (commands / chat). */
#define HYBBX_LINE_MAX 80u

/** Typical AX.25 UI payload ceiling used for outbound chunk sizing. */
#define HYBBX_AX25_PAYLOAD_MAX 256u

typedef struct hybbx_traffic_config {
    unsigned baud;
    unsigned line_width;
    int pace_output;
    int ansi;
} hybbx_traffic_config_t;

void hybbx_traffic_config_defaults(hybbx_traffic_config_t *cfg);
void hybbx_traffic_config_apply(const struct hybbx_config *config);
const hybbx_traffic_config_t *hybbx_traffic_config_get(void);

/**
 * Microseconds to wait after one 8N1 byte at @p baud (0 when pacing is off).
 */
unsigned hybbx_traffic_byte_delay_us(unsigned baud);

/**
 * Emit @p data to @p session with plain-ASCII policy, column wrap, and pacing.
 * @p out_col is updated (wrap position on the current output line).
 */
hybbx_result_t hybbx_traffic_emit(struct hybbx_session *session,
                                  unsigned *out_col,
                                  const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TRAFFIC_H */
