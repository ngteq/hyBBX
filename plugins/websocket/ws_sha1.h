#ifndef HYBBX_WS_SHA1_H
#define HYBBX_WS_SHA1_H

#include <stddef.h>
#include <stdint.h>

void hybbx_ws_sha1(const uint8_t *data, size_t len, uint8_t out[20]);

#endif /* HYBBX_WS_SHA1_H */
