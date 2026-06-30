#ifndef HYBBX_SERIAL_PORT_H
#define HYBBX_SERIAL_PORT_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_serial_port hybbx_serial_port_t;

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 unsigned int baud);

void hybbx_serial_close(hybbx_serial_port_t *port);

hybbx_result_t hybbx_serial_write(hybbx_serial_port_t *port,
                                  const uint8_t *data, size_t len);

hybbx_result_t hybbx_serial_read(hybbx_serial_port_t *port,
                                 uint8_t *buf, size_t buf_len,
                                 size_t *read_len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SERIAL_PORT_H */
