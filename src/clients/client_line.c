#include "client_line.h"

#include "hybbx/util.h"

#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define CLIENT_ESC_NONE 0u
#define CLIENT_ESC_ESC  1u
#define CLIENT_ESC_CSI  2u
#define CLIENT_ESC_SS3  3u

static int history_should_store(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    return strncmp(line, "/login ", 7) != 0 &&
           strncmp(line, "/register ", 10) != 0 &&
           strncmp(line, "/changeme ", 10) != 0 &&
           strncmp(line, "/userchange ", 12) != 0;
}

static hybbx_result_t write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_ERR_IO;
        }
        off += (size_t)n;
    }

    return HYBBX_OK;
}

static hybbx_result_t move_back(hybbx_client_line_editor_t *ed, size_t count)
{
    char buf[64];
    size_t sent = 0;

    if (ed == NULL || !ed->raw_enabled || count == 0) {
        return HYBBX_OK;
    }

    memset(buf, '\b', sizeof(buf));
    while (sent < count) {
        size_t chunk = count - sent;

        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }
        if (write_all(STDOUT_FILENO, buf, chunk) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        sent += chunk;
    }

    return HYBBX_OK;
}

static hybbx_result_t refresh_suffix(hybbx_client_line_editor_t *ed)
{
    size_t suffix_len;

    if (ed == NULL || !ed->raw_enabled) {
        return HYBBX_OK;
    }

    suffix_len = ed->line_len - ed->line_cursor;
    if (suffix_len > 0 &&
        write_all(STDOUT_FILENO, ed->line + ed->line_cursor, suffix_len) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    if (write_all(STDOUT_FILENO, "\x1b[K", 3) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    return move_back(ed, suffix_len);
}

static hybbx_result_t cursor_left(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL || ed->line_cursor == 0) {
        return HYBBX_OK;
    }
    ed->line_cursor--;
    return ed->raw_enabled ? write_all(STDOUT_FILENO, "\b", 1) : HYBBX_OK;
}

static hybbx_result_t cursor_right(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL || ed->line_cursor >= ed->line_len) {
        return HYBBX_OK;
    }
    if (ed->raw_enabled &&
        write_all(STDOUT_FILENO, ed->line + ed->line_cursor, 1) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    ed->line_cursor++;
    return HYBBX_OK;
}

static hybbx_result_t cursor_home(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL) {
        return HYBBX_ERR_INVALID;
    }
    return move_back(ed, ed->line_cursor);
}

static hybbx_result_t cursor_end(hybbx_client_line_editor_t *ed)
{
    while (ed != NULL && ed->line_cursor < ed->line_len) {
        if (cursor_right(ed) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
    }
    return HYBBX_OK;
}

static hybbx_result_t delete_forward(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL || ed->line_cursor >= ed->line_len) {
        return HYBBX_OK;
    }

    memmove(ed->line + ed->line_cursor, ed->line + ed->line_cursor + 1,
            ed->line_len - ed->line_cursor);
    ed->line_len--;
    ed->line[ed->line_len] = '\0';
    return refresh_suffix(ed);
}

static hybbx_result_t backspace_char(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL || ed->line_cursor == 0) {
        return HYBBX_OK;
    }

    ed->line_cursor--;
    memmove(ed->line + ed->line_cursor, ed->line + ed->line_cursor + 1,
            ed->line_len - ed->line_cursor);
    ed->line_len--;
    ed->line[ed->line_len] = '\0';
    if (ed->raw_enabled && write_all(STDOUT_FILENO, "\b", 1) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    return refresh_suffix(ed);
}

static hybbx_result_t insert_char(hybbx_client_line_editor_t *ed, char ch)
{
    if (ed == NULL || (unsigned char)ch < 0x20u || ch == 0x7f) {
        return HYBBX_OK;
    }
    if (ed->line_len + 1 >= sizeof(ed->line)) {
        return HYBBX_OK;
    }

    if (ed->line_cursor < ed->line_len) {
        memmove(ed->line + ed->line_cursor + 1, ed->line + ed->line_cursor,
                ed->line_len - ed->line_cursor);
    }
    ed->line[ed->line_cursor] = ch;
    ed->line_len++;
    ed->line_cursor++;
    ed->line[ed->line_len] = '\0';

    if (!ed->raw_enabled) {
        return HYBBX_OK;
    }
    if (write_all(STDOUT_FILENO, &ch, 1) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    return refresh_suffix(ed);
}

static void history_add(hybbx_client_line_editor_t *ed, const char *line)
{
    if (ed == NULL || !history_should_store(line)) {
        return;
    }

    hybbx_strlcpy(ed->history[ed->history_next], line, sizeof(ed->history[0]));
    ed->history_next = (ed->history_next + 1u) % HYBBX_HISTORY_MAX;
    if (ed->history_count < HYBBX_HISTORY_MAX) {
        ed->history_count++;
    }
    ed->history_view = -1;
    ed->history_saved_line[0] = '\0';
}

static hybbx_result_t line_replace(hybbx_client_line_editor_t *ed, const char *line)
{
    size_t len;

    if (ed == NULL || line == NULL) {
        return HYBBX_ERR_INVALID;
    }

    len = strlen(line);
    if (len >= sizeof(ed->line)) {
        len = sizeof(ed->line) - 1;
    }

    if (ed->raw_enabled) {
        if (move_back(ed, ed->line_cursor) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        if (len > 0 && write_all(STDOUT_FILENO, line, len) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        if (write_all(STDOUT_FILENO, "\x1b[K", 3) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
    }

    memcpy(ed->line, line, len);
    ed->line[len] = '\0';
    ed->line_len = len;
    ed->line_cursor = len;
    return HYBBX_OK;
}

static hybbx_result_t history_up(hybbx_client_line_editor_t *ed)
{
    unsigned slot;

    if (ed == NULL || ed->history_count == 0) {
        return HYBBX_OK;
    }

    if (ed->history_view < 0) {
        hybbx_strlcpy(ed->history_saved_line, ed->line, sizeof(ed->history_saved_line));
        ed->history_view = 0;
    } else if ((unsigned)(ed->history_view + 1) < ed->history_count) {
        ed->history_view++;
    } else {
        return HYBBX_OK;
    }

    slot = (ed->history_next + HYBBX_HISTORY_MAX - 1u -
            (unsigned)ed->history_view) % HYBBX_HISTORY_MAX;
    return line_replace(ed, ed->history[slot]);
}

static hybbx_result_t history_down(hybbx_client_line_editor_t *ed)
{
    unsigned slot;

    if (ed == NULL || ed->history_view < 0) {
        return HYBBX_OK;
    }
    if (ed->history_view == 0) {
        ed->history_view = -1;
        return line_replace(ed, ed->history_saved_line);
    }

    ed->history_view--;
    slot = (ed->history_next + HYBBX_HISTORY_MAX - 1u -
            (unsigned)ed->history_view) % HYBBX_HISTORY_MAX;
    return line_replace(ed, ed->history[slot]);
}

static void escape_reset(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL) {
        return;
    }
    ed->esc_state = CLIENT_ESC_NONE;
    ed->csi_len = 0;
}

static int csi_is_final(unsigned char ch)
{
    return ch >= 0x40u && ch <= 0x7eu;
}

static hybbx_result_t apply_csi(hybbx_client_line_editor_t *ed,
                                const char *params, char final_ch)
{
    (void)params;

    if (final_ch == 'A') {
        return history_up(ed);
    }
    if (final_ch == 'B') {
        return history_down(ed);
    }
    if (final_ch == 'C') {
        return cursor_right(ed);
    }
    if (final_ch == 'D') {
        return cursor_left(ed);
    }
    if (final_ch == 'H') {
        return cursor_home(ed);
    }
    if (final_ch == 'F') {
        return cursor_end(ed);
    }
    if (final_ch == '~') {
        if (strcmp(params, "1") == 0 || strcmp(params, "7") == 0) {
            return cursor_home(ed);
        }
        if (strcmp(params, "3") == 0) {
            return delete_forward(ed);
        }
        if (strcmp(params, "4") == 0 || strcmp(params, "8") == 0) {
            return cursor_end(ed);
        }
    }
    return HYBBX_OK;
}

static int filter_escape_byte(hybbx_client_line_editor_t *ed, unsigned char byte)
{
    if (ed == NULL) {
        return 0;
    }
    if (ed->esc_state == CLIENT_ESC_NONE && byte == 0x9bu) {
        ed->esc_state = CLIENT_ESC_CSI;
        ed->csi_len = 0;
        return 1;
    }
    if (ed->esc_state == CLIENT_ESC_NONE && byte == 0x1bu) {
        ed->esc_state = CLIENT_ESC_ESC;
        return 1;
    }
    if (ed->esc_state == CLIENT_ESC_ESC) {
        if (byte == '[') {
            ed->esc_state = CLIENT_ESC_CSI;
            ed->csi_len = 0;
            return 1;
        }
        if (byte == 'O') {
            ed->esc_state = CLIENT_ESC_SS3;
            return 1;
        }
        escape_reset(ed);
        return 0;
    }
    if (ed->esc_state == CLIENT_ESC_SS3) {
        if (byte == 'C') {
            (void)cursor_right(ed);
        } else if (byte == 'D') {
            (void)cursor_left(ed);
        } else if (byte == 'H') {
            (void)cursor_home(ed);
        } else if (byte == 'F') {
            (void)cursor_end(ed);
        }
        escape_reset(ed);
        return 1;
    }
    if (ed->esc_state == CLIENT_ESC_CSI) {
        char params[24];

        if (ed->csi_len + 1 >= sizeof(ed->csi_buf)) {
            escape_reset(ed);
            return 1;
        }
        ed->csi_buf[ed->csi_len++] = (char)byte;
        if (!csi_is_final(byte)) {
            return 1;
        }

        if (ed->csi_len > 1) {
            memcpy(params, ed->csi_buf, ed->csi_len - 1);
            params[ed->csi_len - 1] = '\0';
        } else {
            params[0] = '\0';
        }
        escape_reset(ed);
        (void)apply_csi(ed, params, (char)byte);
        return 1;
    }
    return 0;
}

int hybbx_client_line_editor_start(hybbx_client_line_editor_t *ed, int stdin_fd)
{
    struct termios raw;

    if (ed == NULL) {
        return -1;
    }

    memset(ed, 0, sizeof(*ed));
    ed->stdin_fd = stdin_fd;
    ed->history_view = -1;

    if (!isatty(stdin_fd)) {
        return 0;
    }
    if (tcgetattr(stdin_fd, &ed->termios_saved) != 0) {
        return -1;
    }

    raw = ed->termios_saved;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(stdin_fd, TCSAFLUSH, &raw) != 0) {
        return -1;
    }

    ed->raw_enabled = 1;
    return 0;
}

void hybbx_client_line_editor_stop(hybbx_client_line_editor_t *ed)
{
    if (ed == NULL || !ed->raw_enabled) {
        return;
    }
    (void)tcsetattr(ed->stdin_fd, TCSAFLUSH, &ed->termios_saved);
    ed->raw_enabled = 0;
}

hybbx_result_t hybbx_client_line_editor_poll(hybbx_client_line_editor_t *ed,
                                             char *out_line, size_t out_len,
                                             int *have_line, int *eof_seen)
{
    unsigned char buf[64];
    ssize_t n;
    size_t i;

    if (ed == NULL || out_line == NULL || out_len == 0 ||
        have_line == NULL || eof_seen == NULL) {
        return HYBBX_ERR_INVALID;
    }

    *have_line = 0;
    *eof_seen = 0;

    n = read(ed->stdin_fd, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EINTR) {
            return HYBBX_OK;
        }
        return HYBBX_ERR_IO;
    }
    if (n == 0) {
        *eof_seen = 1;
        return HYBBX_OK;
    }

    for (i = 0; i < (size_t)n; i++) {
        unsigned char ch = buf[i];

        if (filter_escape_byte(ed, ch)) {
            continue;
        }
        if (ch == '\b' || ch == 127) {
            if (backspace_char(ed) != HYBBX_OK) {
                return HYBBX_ERR_IO;
            }
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (ed->raw_enabled &&
                write_all(STDOUT_FILENO, "\r\n", 2) != HYBBX_OK) {
                return HYBBX_ERR_IO;
            }
            hybbx_strlcpy(out_line, ed->line, out_len);
            history_add(ed, ed->line);
            ed->line[0] = '\0';
            ed->line_len = 0;
            ed->line_cursor = 0;
            ed->history_view = -1;
            *have_line = 1;
            return HYBBX_OK;
        }
        if (insert_char(ed, (char)ch) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
    }

    return HYBBX_OK;
}
