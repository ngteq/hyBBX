#include "hybbx/hybbx.h"
#include "hybbx/service.h"
#include "hybbx/registry.h"
#include "hybbx/config.h"
#include "hybbx/command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HYBBX_HAVE_PLUGIN_TELNET
extern const hybbx_transport_plugin_t hybbx_plugin_telnet;
#endif
#ifdef HYBBX_HAVE_PLUGIN_PACKET_RADIO
extern const hybbx_transport_plugin_t hybbx_plugin_packet_radio;
#endif

static void register_builtin_plugins(void)
{
#ifdef HYBBX_HAVE_PLUGIN_TELNET
    hybbx_registry_register(&hybbx_plugin_telnet);
#endif
#ifdef HYBBX_HAVE_PLUGIN_PACKET_RADIO
    hybbx_registry_register(&hybbx_plugin_packet_radio);
#endif
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "HyBBX %s — connection service (BBS/mailbox-inspired)\n"
            "\n"
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -c, --config <file>  Load INI configuration file\n"
            "  -h, --help           Show this help\n"
            "  -V, --version        Show version\n"
            "\n"
            "Commands (at runtime):\n"
            "  /help [/command]   HyBBX commands (leading / required)\n"
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

int main(int argc, char *argv[])
{
    hybbx_service_t *service;
    hybbx_config_t config;
    const char *config_path = NULL;
    int have_config = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return EXIT_SUCCESS;
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

    register_builtin_plugins();

    printf("HyBBX %s starting\n", HYBBX_VERSION_STRING);
    printf("Available transport plugins:\n");
    hybbx_registry_foreach(list_plugins_cb, NULL);

    service = hybbx_service_create(NULL);
    if (service == NULL) {
        fprintf(stderr, "Failed to create service\n");
        return EXIT_FAILURE;
    }

    if (config_path != NULL) {
        hybbx_result_t rc = hybbx_config_load(&config, config_path);

        if (rc != HYBBX_OK) {
            fprintf(stderr, "Failed to load config '%s'\n", config_path);
            hybbx_service_destroy(service);
            return EXIT_FAILURE;
        }

        have_config = 1;
        printf("Loaded configuration: %s\n", config_path);

        rc = hybbx_service_apply_config(service, &config, config_path);
        if (rc != HYBBX_OK) {
            fprintf(stderr, "Failed to apply configuration\n");
            hybbx_config_free(&config);
            hybbx_service_destroy(service);
            return EXIT_FAILURE;
        }
    }

    hybbx_service_run(service);

    if (have_config) {
        hybbx_config_free(&config);
    }
    hybbx_service_destroy(service);

    return EXIT_SUCCESS;
}
