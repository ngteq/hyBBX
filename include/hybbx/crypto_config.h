#ifndef HYBBX_CRYPTO_CONFIG_H
#define HYBBX_CRYPTO_CONFIG_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_config;

/** Password hashing backend for new hashes and verification. */
typedef enum hybbx_password_hash_backend {
    HYBBX_PASSWORD_HASH_TINYSHA256 = 1,
    HYBBX_PASSWORD_HASH_OPENSSL = 2
} hybbx_password_hash_backend_t;

/** AES-256-GCM implementation backend. */
typedef enum hybbx_aes_gcm_backend {
    HYBBX_AES_GCM_TINYAES = 1,
    HYBBX_AES_GCM_OPENSSL = 2
} hybbx_aes_gcm_backend_t;

/** XChaCha20-Poly1305 implementation backend. */
typedef enum hybbx_chacha_backend {
    HYBBX_CHACHA_MONOCYPHER = 1,
    HYBBX_CHACHA_LIBSODIUM = 2
} hybbx_chacha_backend_t;

/** X25519 key agreement backend (sealed-box mode). */
typedef enum hybbx_x25519_backend {
    HYBBX_X25519_MONOCYPHER = 1,
    HYBBX_X25519_LIBSODIUM = 2
} hybbx_x25519_backend_t;

/** OS random bytes source. */
typedef enum hybbx_random_backend {
    HYBBX_RANDOM_SYSTEM = 1,
    HYBBX_RANDOM_OPENSSL = 2
} hybbx_random_backend_t;

typedef struct hybbx_crypto_config {
    hybbx_password_hash_backend_t password_hash;
    hybbx_aes_gcm_backend_t aes_gcm;
    hybbx_chacha_backend_t chacha;
    hybbx_x25519_backend_t x25519;
    hybbx_random_backend_t random;
} hybbx_crypto_config_t;

/** Bundled defaults: tinysha256, tinyaes, monocypher, system random. */
void hybbx_crypto_config_defaults(hybbx_crypto_config_t *cfg);

/** Load `[crypto]` from @p config (missing keys keep defaults). */
void hybbx_crypto_config_apply(const struct hybbx_config *config);

/** Active system-wide crypto settings (defaults until apply is called). */
const hybbx_crypto_config_t *hybbx_crypto_config_get(void);

const char *hybbx_password_hash_backend_name(hybbx_password_hash_backend_t b);
const char *hybbx_aes_gcm_backend_name(hybbx_aes_gcm_backend_t b);
const char *hybbx_chacha_backend_name(hybbx_chacha_backend_t b);
const char *hybbx_x25519_backend_name(hybbx_x25519_backend_t b);
const char *hybbx_random_backend_name(hybbx_random_backend_t b);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CRYPTO_CONFIG_H */
