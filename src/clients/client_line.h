#ifndef HYBBX_CLIENT_LINE_H
#define HYBBX_CLIENT_LINE_H

#include "hybbx/limits.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <termios.h>

typedef struct hybbx_client_line_editor {
    char line[HYBBX_LINE_MAX];
    size_t line_len;
    size_t line_cursor;
    char history[HYBBX_HISTORY_MAX][HYBBX_LINE_MAX];
    unsigned history_count;
    unsigned history_next;
    int history_view;
    char history_saved_line[HYBBX_LINE_MAX];
    int raw_enabled;
    int stdin_fd;
    unsigned char esc_state;
    char csi_buf[24];
    size_t csi_len;
    struct termios termios_saved;
} hybbx_client_line_editor_t;

int hybbx_client_line_editor_start(hybbx_client_line_editor_t *ed, int stdin_fd);
void hybbx_client_line_editor_stop(hybbx_client_line_editor_t *ed);
hybbx_result_t hybbx_client_line_editor_poll(hybbx_client_line_editor_t *ed,
                                             char *out_line, size_t out_len,
                                             int *have_line, int *eof_seen);

#endif /* HYBBX_CLIENT_LINE_H */
