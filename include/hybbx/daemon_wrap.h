#ifndef HYBBX_DAEMON_WRAP_H
#define HYBBX_DAEMON_WRAP_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_DAEMON_DEFAULT_SESSION "hybbxd"
#define HYBBX_DAEMON_WRAP_ENV "HYBBXD_WRAPPED"

typedef struct hybbx_daemon_launch_opts {
    int foreground;
    int attach;
    int use_screen;
    int use_tmux;
    char session[32];
} hybbx_daemon_launch_opts_t;

void hybbx_daemon_launch_opts_defaults(hybbx_daemon_launch_opts_t *opts);

/**
 * Handle --screen / --tmux / --attach before the service loop.
 * Spawns or attaches via GNU screen or tmux when requested.
 * Returns HYBBX_OK to continue a foreground run; otherwise exits the process.
 */
hybbx_result_t hybbx_daemon_apply_launch_opts(const hybbx_daemon_launch_opts_t *opts,
                                              const char *binary_path,
                                              int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_DAEMON_WRAP_H */
