#include "hybbx/terminal.h"
#include "hybbx/session.h"
#include "hybbx/traffic.h"
#include "hybbx/limits.h"

#include <string.h>

static int term_is_csi_terminator(unsigned char ch)
{
    return ch >= 0x40 && ch <= 0x7E;
}

size_t hybbx_term_copy_plain(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;
    size_t si = 0;

    if (dst == NULL || dst_size == 0) {
        return 0;
    }

    dst[0] = '\0';

    if (src == NULL) {
        return 0;
    }

    while (src[si] != '\0' && di + 1 < dst_size) {
        unsigned char ch = (unsigned char)src[si];

        if (ch == 0x1Bu) {
            si++;
            if (src[si] == '\0') {
                break;
            }

            if (src[si] == '[') {
                si++;
                while (src[si] != '\0' && !term_is_csi_terminator((unsigned char)src[si])) {
                    si++;
                }
                if (src[si] != '\0') {
                    si++;
                }
            } else {
                si++;
            }
            continue;
        }

        dst[di++] = (char)ch;
        si++;
    }

    dst[di] = '\0';
    return di;
}

hybbx_result_t hybbx_term_init_session(hybbx_session_t *session)
{
    const hybbx_traffic_config_t *traffic;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    traffic = hybbx_traffic_config_get();
    if (traffic == NULL || !traffic->ansi) {
        return HYBBX_OK;
    }

    return hybbx_session_write(session, HYBBX_TERM_SGR_LIGHTGRAY_ON_BLACK);
}

hybbx_result_t hybbx_term_clear_screen(hybbx_session_t *session)
{
    const hybbx_traffic_config_t *traffic;
    int i;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    traffic = hybbx_traffic_config_get();
    if (traffic != NULL && traffic->ansi) {
        return hybbx_session_write(session, HYBBX_TERM_CLEAR_SCREEN);
    }

    for (i = 0; i < 24; i++) {
        hybbx_result_t rc = hybbx_session_write(session, "\n");

        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return HYBBX_OK;
}
