#include "hybbx/ssh.h"
#include "hybbx/util.h"

#include <libssh/libssh.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static hybbx_result_t mkdir_keys_dir(const char *keys_dir)
{
    char parent[HYBBX_PATH_MAX];
    struct stat st;

    if (keys_dir == NULL || keys_dir[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (stat(keys_dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? HYBBX_OK : HYBBX_ERR_IO;
    }

    if (hybbx_path_dirname(keys_dir, parent, sizeof(parent)) == HYBBX_OK &&
        parent[0] != '\0' && strcmp(parent, keys_dir) != 0 &&
        stat(parent, &st) != 0) {
        if (mkdir_keys_dir(parent) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
    }

    if (mkdir(keys_dir, 0700) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static hybbx_result_t generate_ed25519_keypair(const char *priv_path,
                                               const char *pub_path)
{
    ssh_key key = NULL;
    int rc;

    rc = ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &key);
    if (rc != SSH_OK || key == NULL) {
        fprintf(stderr, "[ssh] Ed25519 generate failed (rc=%d)\n", rc);
        return HYBBX_ERR_IO;
    }

    rc = ssh_pki_export_privkey_file(key, NULL, NULL, NULL, priv_path);
    if (rc != SSH_OK) {
        fprintf(stderr, "[ssh] export private key failed: %s\n",
                ssh_get_error(key));
        ssh_key_free(key);
        return HYBBX_ERR_IO;
    }

    rc = ssh_pki_export_pubkey_file(key, pub_path);
    ssh_key_free(key);
    if (rc != SSH_OK) {
        fprintf(stderr, "[ssh] export public key failed\n");
        return HYBBX_ERR_IO;
    }

    (void)chmod(priv_path, 0600);
    (void)chmod(pub_path, 0644);
    return HYBBX_OK;
}

static int hostkey_needs_rotation(const char *priv_path)
{
    struct stat st;
    time_t now;
    time_t age;

    if (stat(priv_path, &st) != 0) {
        return 0;
    }

    now = time(NULL);
    if (now <= st.st_mtime) {
        return 0;
    }

    age = now - st.st_mtime;
    return age > (time_t)HYBBX_SSH_HOSTKEY_VALID_DAYS * 86400L;
}

hybbx_result_t hybbx_ssh_keys_ensure(const char *keys_dir,
                                     char *hostkey_path,
                                     size_t hostkey_path_len)
{
    char resolved_dir[HYBBX_PATH_MAX];
    char priv_path[HYBBX_PATH_MAX];
    char pub_path[HYBBX_PATH_MAX];
    struct stat st;

    if (keys_dir == NULL || hostkey_path == NULL || hostkey_path_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (ssh_init() < 0) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_resolve(resolved_dir, sizeof(resolved_dir),
                         keys_dir) != HYBBX_OK) {
        hybbx_strlcpy(resolved_dir, keys_dir, sizeof(resolved_dir));
    }

    if (mkdir_keys_dir(resolved_dir) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_join(priv_path, sizeof(priv_path), resolved_dir,
                        HYBBX_SSH_HOSTKEY_ED25519) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_path_join(pub_path, sizeof(pub_path), resolved_dir,
                        HYBBX_SSH_HOSTKEY_ED25519 ".pub") != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    if (stat(priv_path, &st) == 0 && hostkey_needs_rotation(priv_path)) {
        fprintf(stderr,
                "[ssh] host key older than %u days — rotating %s\n",
                HYBBX_SSH_HOSTKEY_VALID_DAYS, priv_path);
        (void)unlink(priv_path);
        (void)unlink(pub_path);
    }

    if (stat(priv_path, &st) != 0) {
        if (generate_ed25519_keypair(priv_path, pub_path) != HYBBX_OK) {
            fprintf(stderr, "[ssh] failed to generate host key in %s\n",
                    resolved_dir);
            return HYBBX_ERR_IO;
        }
        printf("[ssh] generated host key %s (valid %u days)\n", priv_path,
               HYBBX_SSH_HOSTKEY_VALID_DAYS);
    }

    hybbx_strlcpy(hostkey_path, priv_path, hostkey_path_len);
    return HYBBX_OK;
}
