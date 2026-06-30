#include "hybbx/crypto_config.h"
#include "hybbx/config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static hybbx_crypto_config_t g_crypto_config;
static int g_crypto_config_ready;

static int str_ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = (char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);
        char cb = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);

        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static hybbx_password_hash_backend_t parse_password_hash(const char *value)
{
    if (value == NULL || value[0] == '\0' ||
        str_ieq(value, "tinysha256") || str_ieq(value, "bundled") ||
        str_ieq(value, "default")) {
        return HYBBX_PASSWORD_HASH_TINYSHA256;
    }

    if (str_ieq(value, "openssl") || str_ieq(value, "libcrypto")) {
        return HYBBX_PASSWORD_HASH_OPENSSL;
    }

    return HYBBX_PASSWORD_HASH_TINYSHA256;
}

static hybbx_aes_gcm_backend_t parse_aes_gcm(const char *value)
{
    if (value == NULL || value[0] == '\0' ||
        str_ieq(value, "tinyaes") || str_ieq(value, "bundled") ||
        str_ieq(value, "default")) {
        return HYBBX_AES_GCM_TINYAES;
    }

    if (str_ieq(value, "openssl") || str_ieq(value, "libcrypto")) {
        return HYBBX_AES_GCM_OPENSSL;
    }

    return HYBBX_AES_GCM_TINYAES;
}

static hybbx_chacha_backend_t parse_chacha(const char *value)
{
    if (value == NULL || value[0] == '\0' ||
        str_ieq(value, "monocypher") || str_ieq(value, "bundled") ||
        str_ieq(value, "default")) {
        return HYBBX_CHACHA_MONOCYPHER;
    }

    if (str_ieq(value, "libsodium") || str_ieq(value, "sodium")) {
        return HYBBX_CHACHA_LIBSODIUM;
    }

    return HYBBX_CHACHA_MONOCYPHER;
}

static hybbx_x25519_backend_t parse_x25519(const char *value)
{
    if (value == NULL || value[0] == '\0' ||
        str_ieq(value, "monocypher") || str_ieq(value, "bundled") ||
        str_ieq(value, "default")) {
        return HYBBX_X25519_MONOCYPHER;
    }

    if (str_ieq(value, "libsodium") || str_ieq(value, "sodium")) {
        return HYBBX_X25519_LIBSODIUM;
    }

    return HYBBX_X25519_MONOCYPHER;
}

static hybbx_random_backend_t parse_random(const char *value)
{
    if (value == NULL || value[0] == '\0' ||
        str_ieq(value, "system") || str_ieq(value, "bundled") ||
        str_ieq(value, "default") || str_ieq(value, "getrandom") ||
        str_ieq(value, "urandom")) {
        return HYBBX_RANDOM_SYSTEM;
    }

    if (str_ieq(value, "openssl") || str_ieq(value, "libcrypto")) {
        return HYBBX_RANDOM_OPENSSL;
    }

    return HYBBX_RANDOM_SYSTEM;
}

static void resolve_backends(hybbx_crypto_config_t *cfg)
{
#if !defined(HYBBX_HAVE_OPENSSL)
    if (cfg->password_hash == HYBBX_PASSWORD_HASH_OPENSSL) {
        fprintf(stderr,
                "[crypto] password_hash=openssl requested but HyBBX was not "
                "built with OpenSSL; using tinysha256\n");
        cfg->password_hash = HYBBX_PASSWORD_HASH_TINYSHA256;
    }
    if (cfg->aes_gcm == HYBBX_AES_GCM_OPENSSL) {
        fprintf(stderr,
                "[crypto] aes_gcm=openssl requested but HyBBX was not built "
                "with OpenSSL; using tinyaes\n");
        cfg->aes_gcm = HYBBX_AES_GCM_TINYAES;
    }
    if (cfg->random == HYBBX_RANDOM_OPENSSL) {
        fprintf(stderr,
                "[crypto] random=openssl requested but HyBBX was not built "
                "with OpenSSL; using system\n");
        cfg->random = HYBBX_RANDOM_SYSTEM;
    }
#endif

#if !defined(HYBBX_HAVE_LIBSODIUM)
    if (cfg->chacha == HYBBX_CHACHA_LIBSODIUM) {
        fprintf(stderr,
                "[crypto] chacha=libsodium requested but HyBBX was not built "
                "with libsodium; using monocypher\n");
        cfg->chacha = HYBBX_CHACHA_MONOCYPHER;
    }
    if (cfg->x25519 == HYBBX_X25519_LIBSODIUM) {
        fprintf(stderr,
                "[crypto] x25519=libsodium requested but HyBBX was not built "
                "with libsodium; using monocypher\n");
        cfg->x25519 = HYBBX_X25519_MONOCYPHER;
    }
#endif
}

void hybbx_crypto_config_defaults(hybbx_crypto_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->password_hash = HYBBX_PASSWORD_HASH_TINYSHA256;
    cfg->aes_gcm = HYBBX_AES_GCM_TINYAES;
    cfg->chacha = HYBBX_CHACHA_MONOCYPHER;
    cfg->x25519 = HYBBX_X25519_MONOCYPHER;
    cfg->random = HYBBX_RANDOM_SYSTEM;
}

void hybbx_crypto_config_apply(const hybbx_config_t *config)
{
    const char *value;

    hybbx_crypto_config_defaults(&g_crypto_config);

    if (config != NULL) {
        value = hybbx_config_get(config, "crypto", "password_hash", NULL);
        g_crypto_config.password_hash = parse_password_hash(value);

        value = hybbx_config_get(config, "crypto", "aes_gcm", NULL);
        g_crypto_config.aes_gcm = parse_aes_gcm(value);

        value = hybbx_config_get(config, "crypto", "chacha", NULL);
        g_crypto_config.chacha = parse_chacha(value);

        value = hybbx_config_get(config, "crypto", "x25519", NULL);
        g_crypto_config.x25519 = parse_x25519(value);

        value = hybbx_config_get(config, "crypto", "random", NULL);
        g_crypto_config.random = parse_random(value);
    }

    resolve_backends(&g_crypto_config);
    g_crypto_config_ready = 1;

    printf("[crypto] password_hash=%s aes_gcm=%s chacha=%s x25519=%s random=%s\n",
           hybbx_password_hash_backend_name(g_crypto_config.password_hash),
           hybbx_aes_gcm_backend_name(g_crypto_config.aes_gcm),
           hybbx_chacha_backend_name(g_crypto_config.chacha),
           hybbx_x25519_backend_name(g_crypto_config.x25519),
           hybbx_random_backend_name(g_crypto_config.random));
}

const hybbx_crypto_config_t *hybbx_crypto_config_get(void)
{
    if (!g_crypto_config_ready) {
        hybbx_crypto_config_defaults(&g_crypto_config);
        g_crypto_config_ready = 1;
    }

    return &g_crypto_config;
}

const char *hybbx_password_hash_backend_name(hybbx_password_hash_backend_t b)
{
    switch (b) {
    case HYBBX_PASSWORD_HASH_OPENSSL:
        return "openssl";
    default:
        return "tinysha256";
    }
}

const char *hybbx_aes_gcm_backend_name(hybbx_aes_gcm_backend_t b)
{
    switch (b) {
    case HYBBX_AES_GCM_OPENSSL:
        return "openssl";
    default:
        return "tinyaes";
    }
}

const char *hybbx_chacha_backend_name(hybbx_chacha_backend_t b)
{
    switch (b) {
    case HYBBX_CHACHA_LIBSODIUM:
        return "libsodium";
    default:
        return "monocypher";
    }
}

const char *hybbx_x25519_backend_name(hybbx_x25519_backend_t b)
{
    switch (b) {
    case HYBBX_X25519_LIBSODIUM:
        return "libsodium";
    default:
        return "monocypher";
    }
}

const char *hybbx_random_backend_name(hybbx_random_backend_t b)
{
    switch (b) {
    case HYBBX_RANDOM_OPENSSL:
        return "openssl";
    default:
        return "system";
    }
}
