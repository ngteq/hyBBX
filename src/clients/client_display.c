#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "client_display.h"

#include "hybbx/terminal.h"
#include "hybbx/traffic.h"

#include <stdio.h>
#include <unistd.h>

void hybbx_client_display_init(hybbx_client_display_t *disp,
                               const hybbx_traffic_config_t *traffic)
{
    if (disp == NULL) {
        return;
    }

    disp->traffic = traffic != NULL ? traffic : hybbx_traffic_config_get();
    disp->col = 0;
}

static void pace_byte(unsigned baud)
{
    unsigned delay_us;

    if (baud == 0) {
        return;
    }

    delay_us = hybbx_traffic_byte_delay_us(baud);
    if (delay_us > 0) {
        usleep(delay_us);
    }
}

void hybbx_client_display_byte(hybbx_client_display_t *disp, uint8_t byte)
{
    const hybbx_traffic_config_t *cfg;

    if (disp == NULL) {
        return;
    }

    cfg = disp->traffic;
    if (cfg == NULL) {
        fputc((int)byte, stdout);
        fflush(stdout);
        return;
    }

    if (!cfg->ansi && byte == 0x1b) {
        return;
    }

    if (cfg->line_width > 0 && (byte == '\n' || byte == '\r')) {
        disp->col = 0;
    } else if (cfg->line_width > 0 && byte >= 0x20 && byte != 0x7f) {
        disp->col++;
        if (disp->col > cfg->line_width) {
            fputc('\n', stdout);
            disp->col = 1;
        }
    }

    fputc((int)byte, stdout);
    fflush(stdout);

    if (cfg->pace_output) {
        pace_byte(cfg->baud);
    }
}

void hybbx_client_display_write(hybbx_client_display_t *disp,
                                const uint8_t *data, size_t len)
{
    size_t i;

    if (disp == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        hybbx_client_display_byte(disp, data[i]);
    }
}
