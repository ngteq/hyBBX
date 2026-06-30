#include "crypto_backends.h"
#include "hybbx/crypto_config.h"

#include "aes256gcm.h"
#include "monocypher.h"
#include "tinysha256.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/random.h>
#endif

#if defined(HYBBX_HAVE_OPENSSL)
void hybbx_openssl_sha256_hex(const char *data, size_t len, char hex[65]);
int hybbx_openssl_aes256gcm_encrypt(
    const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
    size_t aad_len, const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *ciphertext, uint8_t tag[16]);
int hybbx_openssl_aes256gcm_decrypt(
    const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
    size_t aad_len, const uint8_t *ciphertext, size_t ciphertext_len,
    uint8_t *plaintext, const uint8_t tag[16]);
hybbx_result_t hybbx_openssl_random(uint8_t *buf, size_t len);
#endif

#if defined(HYBBX_HAVE_LIBSODIUM)
void hybbx_libsodium_chacha_lock(uint8_t *ciphertext, uint8_t tag[16],
                                 const uint8_t key[32], const uint8_t nonce[24],
                                 const uint8_t *aad, size_t aad_len,
                                 const uint8_t *plaintext, size_t plaintext_len);
int hybbx_libsodium_chacha_unlock(uint8_t *plaintext, const uint8_t tag[16],
                                  const uint8_t key[32], const uint8_t nonce[24],
                                  const uint8_t *aad, size_t aad_len,
                                  const uint8_t *ciphertext, size_t ciphertext_len);
void hybbx_libsodium_x25519_public_key(uint8_t public_key[32],
                                       const uint8_t secret_key[32]);
void hybbx_libsodium_x25519_shared(uint8_t shared[32],
                                   const uint8_t secret_key[32],
                                   const uint8_t peer_public_key[32]);
#endif

void hybbx_backend_sha256_hex(const char *data, size_t len, char hex[65])
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_OPENSSL)
    if (cfg->password_hash == HYBBX_PASSWORD_HASH_OPENSSL) {
        hybbx_openssl_sha256_hex(data, len, hex);
        return;
    }
#endif

    tinysha256_hex(data, len, hex);
}

int hybbx_backend_aes256gcm_encrypt(
    const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
    size_t aad_len, const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *ciphertext, uint8_t tag[16])
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_OPENSSL)
    if (cfg->aes_gcm == HYBBX_AES_GCM_OPENSSL) {
        return hybbx_openssl_aes256gcm_encrypt(key, nonce, aad, aad_len,
                                                 plaintext, plaintext_len,
                                                 ciphertext, tag);
    }
#endif

    return hybbx_aes256gcm_encrypt(key, nonce, aad, aad_len, plaintext,
                                   plaintext_len, ciphertext, tag);
}

int hybbx_backend_aes256gcm_decrypt(
    const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
    size_t aad_len, const uint8_t *ciphertext, size_t ciphertext_len,
    uint8_t *plaintext, const uint8_t tag[16])
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_OPENSSL)
    if (cfg->aes_gcm == HYBBX_AES_GCM_OPENSSL) {
        return hybbx_openssl_aes256gcm_decrypt(key, nonce, aad, aad_len,
                                               ciphertext, ciphertext_len,
                                               plaintext, tag);
    }
#endif

    return hybbx_aes256gcm_decrypt(key, nonce, aad, aad_len, ciphertext,
                                   ciphertext_len, plaintext, tag);
}

void hybbx_backend_chacha_lock(uint8_t *ciphertext, uint8_t tag[16],
                               const uint8_t key[32], const uint8_t nonce[24],
                               const uint8_t *aad, size_t aad_len,
                               const uint8_t *plaintext, size_t plaintext_len)
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_LIBSODIUM)
    if (cfg->chacha == HYBBX_CHACHA_LIBSODIUM) {
        hybbx_libsodium_chacha_lock(ciphertext, tag, key, nonce, aad, aad_len,
                                    plaintext, plaintext_len);
        return;
    }
#endif

    crypto_aead_lock(ciphertext, tag, key, nonce, aad, aad_len, plaintext,
                     plaintext_len);
}

int hybbx_backend_chacha_unlock(uint8_t *plaintext, const uint8_t tag[16],
                                const uint8_t key[32], const uint8_t nonce[24],
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t *ciphertext, size_t ciphertext_len)
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_LIBSODIUM)
    if (cfg->chacha == HYBBX_CHACHA_LIBSODIUM) {
        return hybbx_libsodium_chacha_unlock(plaintext, tag, key, nonce, aad,
                                             aad_len, ciphertext,
                                             ciphertext_len);
    }
#endif

    return crypto_aead_unlock(plaintext, tag, key, nonce, aad, aad_len,
                              ciphertext, ciphertext_len);
}

void hybbx_backend_x25519_public_key(uint8_t public_key[32],
                                     const uint8_t secret_key[32])
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_LIBSODIUM)
    if (cfg->x25519 == HYBBX_X25519_LIBSODIUM) {
        hybbx_libsodium_x25519_public_key(public_key, secret_key);
        return;
    }
#endif

    crypto_x25519_public_key(public_key, secret_key);
}

void hybbx_backend_x25519_shared(uint8_t shared[32],
                                 const uint8_t secret_key[32],
                                 const uint8_t peer_public_key[32])
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_LIBSODIUM)
    if (cfg->x25519 == HYBBX_X25519_LIBSODIUM) {
        hybbx_libsodium_x25519_shared(shared, secret_key, peer_public_key);
        return;
    }
#endif

    crypto_x25519(shared, secret_key, peer_public_key);
}

void hybbx_backend_blake2b_keyed(uint8_t *hash, size_t hash_size,
                                 const uint8_t *message, size_t message_size,
                                 const uint8_t *key, size_t key_size)
{
    crypto_blake2b_keyed(hash, hash_size, message, message_size, key, key_size);
}

static hybbx_result_t system_random(uint8_t *buf, size_t len)
{
    size_t done = 0;

#if defined(__linux__)
    while (done < len) {
        ssize_t n = getrandom(buf + done, len - done, 0);

        if (n < 0) {
            break;
        }
        done += (size_t)n;
    }
#endif

    if (done < len) {
        int fd = open("/dev/urandom", O_RDONLY);

        if (fd < 0) {
            return HYBBX_ERR_IO;
        }

        while (done < len) {
            ssize_t n = read(fd, buf + done, len - done);

            if (n <= 0) {
                close(fd);
                return HYBBX_ERR_IO;
            }
            done += (size_t)n;
        }

        close(fd);
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_backend_random(uint8_t *buf, size_t len)
{
    const hybbx_crypto_config_t *cfg = hybbx_crypto_config_get();

#if defined(HYBBX_HAVE_OPENSSL)
    if (cfg->random == HYBBX_RANDOM_OPENSSL) {
        return hybbx_openssl_random(buf, len);
    }
#endif

    return system_random(buf, len);
}
