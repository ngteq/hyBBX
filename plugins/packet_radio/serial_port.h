#ifndef HYBBX_SERIAL_PORT_H
#define HYBBX_SERIAL_PORT_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_serial_port hybbx_serial_port_t;

typedef enum hybbx_serial_parity {
    HYBBX_SERIAL_PARITY_NONE = 0,
    HYBBX_SERIAL_PARITY_EVEN = 1,
    HYBBX_SERIAL_PARITY_ODD = 2
} hybbx_serial_parity_t;

typedef struct hybbx_serial_params {
    unsigned int baud;
    unsigned int data_bits;
    hybbx_serial_parity_t parity;
    unsigned int stop_bits;
    int assert_modem_lines;
} hybbx_serial_params_t;

void hybbx_serial_params_default(hybbx_serial_params_t *params,
                               unsigned int baud);

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 const hybbx_serial_params_t *params);

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
