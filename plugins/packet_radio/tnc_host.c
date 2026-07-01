#include "tnc_host.h"

#include <ctype.h>
#include <string.h>

void hybbx_tnc_host_init(hybbx_tnc_host_t *host,
                         hybbx_tnc_host_text_cb text_cb,
                         hybbx_tnc_host_event_cb event_cb,
                         void *userdata)
{
    if (host == NULL) {
        return;
    }

    memset(host, 0, sizeof(*host));
    host->state = HYBBX_TNC_HOST_BOOT;
    host->text_cb = text_cb;
    host->event_cb = event_cb;
    host->userdata = userdata;
}

void hybbx_tnc_host_reset(hybbx_tnc_host_t *host)
{
    if (host == NULL) {
        return;
    }

    host->line_len = 0;
    host->line[0] = '\0';
    host->state = HYBBX_TNC_HOST_BOOT;
}

static int host_line_ieq(const char *line, const char *prefix)
{
    size_t i;
    size_t plen = strlen(prefix);

    for (i = 0; i < plen; i++) {
        char a = (char)tolower((unsigned char)line[i]);
        char b = (char)tolower((unsigned char)prefix[i]);

        if (a != b) {
            return 0;
        }
    }

    return 1;
}

static void host_emit_event(hybbx_tnc_host_t *host, hybbx_tnc_host_state_t state)
{
    if (host->state == state) {
        return;
    }

    host->state = state;
    if (host->event_cb != NULL) {
        host->event_cb(state, host->userdata);
    }
}

static void host_process_line(hybbx_tnc_host_t *host)
{
    const char *line = host->line;

    if (host_line_ieq(line, "cmd:") || host_line_ieq(line, "cmd: ")) {
        host_emit_event(host, HYBBX_TNC_HOST_CMD);
        return;
    }

    if (strstr(line, "***") != NULL &&
        (host_line_ieq(line, "*** connected") ||
         host_line_ieq(line, "*** connected ") ||
         strstr(line, "CONNECTED") != NULL)) {
        host_emit_event(host, HYBBX_TNC_HOST_CONNECTED);
        return;
    }

    if (strstr(line, "***") != NULL &&
        (host_line_ieq(line, "*** disconnected") ||
         strstr(line, "DISCONNECTED") != NULL)) {
        host_emit_event(host, HYBBX_TNC_HOST_CMD);
        return;
    }

    if (host->state == HYBBX_TNC_HOST_CONNECTED && host->text_cb != NULL &&
        line[0] != '\0') {
        host->text_cb((const uint8_t *)line, strlen(line), host->userdata);
        host->text_cb((const uint8_t *)"\r\n", 2, host->userdata);
    }
}

static void host_push_byte(hybbx_tnc_host_t *host, uint8_t byte)
{
    if (byte == '\r' || byte == '\n') {
        if (host->line_len > 0) {
            host->line[host->line_len] = '\0';
            host_process_line(host);
            host->line_len = 0;
        }
        return;
    }

    if (host->line_len + 1 >= sizeof(host->line)) {
        host->line_len = 0;
        return;
    }

    host->line[host->line_len++] = (char)byte;
}

void hybbx_tnc_host_feed(hybbx_tnc_host_t *host,
                         const uint8_t *data, size_t len)
{
    size_t i;

    if (host == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        host_push_byte(host, data[i]);
    }
}

size_t hybbx_tnc_host_format_cmd(const char *cmd, char *out, size_t out_cap)
{
    size_t len;

    if (cmd == NULL || out == NULL || out_cap < 3) {
        return 0;
    }

    len = strlen(cmd);
    if (len + 2 >= out_cap) {
        return 0;
    }

    memcpy(out, cmd, len);
    out[len++] = '\r';
    out[len] = '\0';
    return len;
}
