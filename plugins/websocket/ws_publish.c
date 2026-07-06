#include "hybbx/websocket.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <string.h>

static void normalize_prefix(char *prefix, size_t len)
{
    size_t n;
    char tmp[HYBBX_WEBSOCKET_PATH_MAX];

    if (prefix == NULL || len == 0) {
        return;
    }

    hybbx_strlcpy(tmp, prefix, sizeof(tmp));
    while (tmp[0] == ' ') {
        memmove(tmp, tmp + 1, strlen(tmp));
    }

    if (tmp[0] != '/') {
        char with_slash[HYBBX_WEBSOCKET_PATH_MAX];

        snprintf(with_slash, sizeof(with_slash), "/%s", tmp);
        hybbx_strlcpy(tmp, with_slash, sizeof(tmp));
    }

    n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') {
        tmp[n - 1] = '\0';
        n--;
    }

    hybbx_strlcpy(prefix, tmp, len);
}

hybbx_result_t hybbx_ws_publish_proxy_config(
    const hybbx_websocket_config_t *config)
{
    char ui_dir[HYBBX_PATH_MAX];
    char cfg_path[HYBBX_PATH_MAX];
    char public_ws[HYBBX_WEBSOCKET_PATH_MAX];
    char prefix[HYBBX_WEBSOCKET_PATH_MAX];
    const char *root;
    FILE *fp;

    if (config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    root = hybbx_install_root_get();
    if (root == NULL || root[0] == '\0') {
        return HYBBX_OK;
    }

    if (hybbx_path_join(ui_dir, sizeof(ui_dir), root,
                        HYBBX_WS_PROXY_UI_DIR) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_join(cfg_path, sizeof(cfg_path), ui_dir,
                        HYBBX_WS_PROXY_CONFIG_FILE) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    hybbx_strlcpy(prefix, config->public_prefix, sizeof(prefix));
    normalize_prefix(prefix, sizeof(prefix));

    if (snprintf(public_ws, sizeof(public_ws), "%s/ws", prefix) >=
        (int)sizeof(public_ws)) {
        return HYBBX_ERR_INVALID;
    }

    fp = fopen(cfg_path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp,
            "{\n"
            "  \"public_prefix\": \"%s\",\n"
            "  \"public_ws\": \"%s\",\n"
            "  \"backend_port\": %u,\n"
            "  \"backend_path\": \"%s\"\n"
            "}\n",
            prefix, public_ws, config->port, config->path);

    if (fclose(fp) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}
