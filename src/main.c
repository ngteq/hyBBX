/*
 * hybbxd — centralized server daemon (main).
 * Plugins: CMake HYBBX_PLUGIN_* ; config: -c path/to/hybbx.ini
 * Standalone clients: src/clients/hybbx-telnet, hybbx-terminal
 */
#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "hybbx/hybbx.h"
#include "hybbx/service.h"
#include "hybbx/registry.h"
#include "hybbx/config.h"
#include "hybbx/command.h"
#include "hybbx/commands_registry.h"
#include "hybbx/daemon_wrap.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#ifdef HYBBX_HAVE_PLUGIN_TELNET
extern const hybbx_transport_plugin_t hybbx_plugin_telnet;
#endif
#ifdef HYBBX_HAVE_PLUGIN_PACKET_RADIO
extern const hybbx_transport_plugin_t hybbx_plugin_packet_radio;
#endif
#ifdef HYBBX_HAVE_PLUGIN_ARDOP
extern const hybbx_transport_plugin_t hybbx_plugin_ardop;
#endif
#ifdef HYBBX_HAVE_PLUGIN_CRDOP
extern const hybbx_transport_plugin_t hybbx_plugin_crdop;
#endif
#ifdef HYBBX_HAVE_PLUGIN_SSH
extern const hybbx_transport_plugin_t hybbx_plugin_ssh;
#endif
#ifdef HYBBX_HAVE_PLUGIN_WEBSOCKET
extern const hybbx_transport_plugin_t hybbx_plugin_websocket;
#endif
#ifdef HYBBX_HAVE_PLUGIN_BAYCOM
extern const hybbx_transport_plugin_t hybbx_plugin_baycom;
#endif
#ifdef HYBBX_HAVE_PLUGIN_MAINS_PROXY
extern const hybbx_transport_plugin_t hybbx_plugin_mains_proxy;
#endif

static void register_builtin_plugins(void)
{
#ifdef HYBBX_HAVE_PLUGIN_TELNET
    hybbx_registry_register(&hybbx_plugin_telnet);
#endif
#ifdef HYBBX_HAVE_PLUGIN_PACKET_RADIO
    hybbx_registry_register(&hybbx_plugin_packet_radio);
#endif
#ifdef HYBBX_HAVE_PLUGIN_ARDOP
    hybbx_registry_register(&hybbx_plugin_ardop);
#endif
#ifdef HYBBX_HAVE_PLUGIN_CRDOP
    hybbx_registry_register(&hybbx_plugin_crdop);
#endif
#ifdef HYBBX_HAVE_PLUGIN_SSH
    hybbx_registry_register(&hybbx_plugin_ssh);
#endif
#ifdef HYBBX_HAVE_PLUGIN_WEBSOCKET
    hybbx_registry_register(&hybbx_plugin_websocket);
#endif
#ifdef HYBBX_HAVE_PLUGIN_BAYCOM
    hybbx_registry_register(&hybbx_plugin_baycom);
#endif
#ifdef HYBBX_HAVE_PLUGIN_MAINS_PROXY
    hybbx_registry_register(&hybbx_plugin_mains_proxy);
#endif
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "HyBBX %s — centralized transport daemon\n"
            "\n"
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -c, --config <file>  Load INI configuration file\n"
            "  -f, --foreground     Run in foreground (default)\n"
            "  --screen [name]       Run detached in GNU screen (default: hybbxd)\n"
            "  --tmux [name]         Run detached in tmux (default: hybbxd)\n"
            "  --attach              Attach to screen/tmux session (with --screen/--tmux)\n"
            "  -h, --help           Show this help\n"
            "  -V, --version        Show version\n"
            "\n"
            "Commands (at runtime):\n"
            "  /help [/command|/cmd|/commands]   HyBBX commands (leading / required)\n"
            "  ; or # ...         Ignored (comment)\n"
            "  other input        Ignored (local, not HyBBX)\n"
            "\n",
            HYBBX_VERSION_STRING, prog);
}

static void print_version(void)
{
    printf("HyBBX %s\n", HYBBX_VERSION_STRING);
}

static void list_plugins_cb(const hybbx_transport_plugin_t *plugin, void *userdata)
{
    (void)userdata;
    printf("  - %s (kind=%d, version=%u)\n",
           plugin->name, (int)plugin->kind, plugin->version);
}

static int path_is_readable(const char *path)
{
    return path != NULL && path[0] != '\0' && access(path, R_OK) == 0;
}

/**
 * When -c is omitted: HYBBX_CONFIG, then <binary>/hybbx.ini (install layout).
 */
static const char *discover_config_path(char *buf, size_t buflen,
                                        const char *argv0)
{
    const char *env;

    if (buf == NULL || buflen == 0) {
        return NULL;
    }

    env = getenv("HYBBX_CONFIG");
    if (path_is_readable(env)) {
        snprintf(buf, buflen, "%s", env);
        return buf;
    }

    if (argv0 == NULL || argv0[0] == '\0') {
        return NULL;
    }

#if defined(__linux__)
    {
        char exe[HYBBX_PATH_MAX];
        char etc[HYBBX_PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

        if (n > 0) {
            char *slash;
            size_t dir_len;

            exe[n] = '\0';
            slash = strrchr(exe, '/');
            if (slash != NULL) {
                dir_len = (size_t)(slash - exe);
                if (dir_len + 1 < sizeof(etc)) {
                    memcpy(etc, exe, dir_len);
                    etc[dir_len] = '\0';
                    if (hybbx_path_join(buf, buflen, etc, HYBBX_FILE_CONFIG) ==
                            HYBBX_OK &&
                        path_is_readable(buf)) {
                        return buf;
                    }
                }
            }
        }
    }
#endif

    snprintf(buf, buflen, "%s", argv0);
    {
        char path_copy[HYBBX_PATH_MAX];
        char candidate[HYBBX_PATH_MAX];
        char *dir;

        snprintf(path_copy, sizeof(path_copy), "%s", argv0);
        dir = dirname(path_copy);
        if (hybbx_path_join(candidate, sizeof(candidate), dir,
                            HYBBX_FILE_CONFIG) == HYBBX_OK &&
            path_is_readable(candidate)) {
            snprintf(buf, buflen, "%s", candidate);
            return buf;
        }
    }

    return NULL;
}

static void setup_install_root(const char *config_path)
{
    const char *env;

    env = getenv(HYBBX_ENV_ROOT);
    if (env != NULL && env[0] != '\0') {
        hybbx_install_root_set(env);
        return;
    }

    if (config_path != NULL && config_path[0] != '\0') {
        char dir[HYBBX_PATH_MAX];

        if (hybbx_path_dirname(config_path, dir, sizeof(dir)) == HYBBX_OK) {
            hybbx_install_root_set(dir);
        }
    }
}

int main(int argc, char *argv[])
{
    hybbx_service_t *service;
    hybbx_config_t config;
    const char *config_path = NULL;
    char discovered_config[HYBBX_PATH_MAX];
    hybbx_daemon_launch_opts_t launch;
    int have_config = 0;
    int i;
    hybbx_result_t rc;

    hybbx_daemon_launch_opts_defaults(&launch);

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--foreground") == 0) {
            launch.foreground = 1;
            continue;
        }
        if (strcmp(argv[i], "--attach") == 0) {
            launch.attach = 1;
            continue;
        }
        if (strcmp(argv[i], "--screen") == 0) {
            launch.use_screen = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-' &&
                strcmp(argv[i + 1], "-c") != 0 &&
                strcmp(argv[i + 1], "--config") != 0) {
                hybbx_strlcpy(launch.session, argv[++i], sizeof(launch.session));
            }
            continue;
        }
        if (strcmp(argv[i], "--tmux") == 0) {
            launch.use_tmux = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-' &&
                strcmp(argv[i + 1], "-c") != 0 &&
                strcmp(argv[i + 1], "--config") != 0) {
                hybbx_strlcpy(launch.session, argv[++i], sizeof(launch.session));
            }
            continue;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing argument for %s\n", argv[i]);
                return EXIT_FAILURE;
            }
            config_path = argv[++i];
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    rc = hybbx_daemon_apply_launch_opts(&launch, argv[0], argc, argv);
    if (rc != HYBBX_OK) {
        return EXIT_FAILURE;
    }

    if (config_path == NULL) {
        config_path = discover_config_path(discovered_config,
                                           sizeof(discovered_config), argv[0]);
    }

    register_builtin_plugins();

    printf("HyBBX %s starting\n", HYBBX_VERSION_STRING);
    printf("Available transport plugins:\n");
    hybbx_registry_foreach(list_plugins_cb, NULL);

    service = hybbx_service_create(NULL);
    if (service == NULL) {
        fprintf(stderr, "Failed to create service (out of memory?)\n");
        return EXIT_FAILURE;
    }

    if (config_path != NULL) {
        rc = hybbx_config_load(&config, config_path);

        if (rc != HYBBX_OK) {
            fprintf(stderr, "Failed to load config '%s'\n", config_path);
            hybbx_service_destroy(service);
            return EXIT_FAILURE;
        }

        have_config = 1;
        printf("Loaded configuration: %s\n", config_path);

        setup_install_root(config_path);

        rc = hybbx_commands_registry_init();
        if (rc != HYBBX_OK) {
            fprintf(stderr, "Failed to load command registry\n");
            hybbx_config_free(&config);
            hybbx_service_destroy(service);
            return EXIT_FAILURE;
        }

        rc = hybbx_service_apply_config(service, &config, config_path);
        if (rc != HYBBX_OK) {
            fprintf(stderr,
                    "Failed to apply configuration (storage path writable? "
                    "telnet/circuit bind ok?) — %s\n",
                    hybbx_result_name(rc));
            hybbx_commands_registry_shutdown();
            hybbx_config_free(&config);
            hybbx_service_destroy(service);
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr,
                "No configuration loaded. Use -c <hybbx.ini>, set HYBBX_CONFIG, "
                "or run hybbx-start.\n");
        fprintf(stderr, "No transports will listen without a config file.\n");
    }

    hybbx_service_set_launch_binary(service, argv[0]);

    hybbx_service_run(service);

    if (hybbx_service_shutdown_mode(service) == HYBBX_SHUTDOWN_RESTART) {
        hybbx_service_restart_exec(service);
    }

    if (have_config) {
        hybbx_config_free(&config);
    }
    hybbx_commands_registry_shutdown();
    hybbx_service_destroy(service);

    return EXIT_SUCCESS;
}
