#ifndef HYBBX_TERMINAL_H
#define HYBBX_TERMINAL_H

#include "hybbx/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_session;

/**
 * HyBBX uses a single terminal palette: light gray text on a black background.
 * ANSI SGR: foreground 37, background 40.
 */
#define HYBBX_TERM_SGR_LIGHTGRAY_ON_BLACK "\033[37;40m"

/** Clear screen and move cursor home (requires `traffic.ansi = yes`). */
#define HYBBX_TERM_CLEAR_SCREEN "\033[2J\033[H"

/** Apply the HyBBX terminal colors on a session (sent once at connect). */
hybbx_result_t hybbx_term_init_session(struct hybbx_session *session);

/** Clear the client screen (ANSI or plain newlines). */
hybbx_result_t hybbx_term_clear_screen(struct hybbx_session *session);

/**
 * Copy @p src to @p dst without ANSI escape sequences (colors, cursor, etc.).
 * @return length of written string excluding the terminator.
 */
size_t hybbx_term_copy_plain(const char *src, char *dst, size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TERMINAL_H */
