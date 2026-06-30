#ifndef HYBBX_CRYPTO_BACKENDS_H
#define HYBBX_CRYPTO_BACKENDS_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_BACKEND_SHA256_HEX_SIZE 65
#define HYBBX_BACKEND_AES_GCM_KEY_SIZE 32
#define HYBBX_BACKEND_AES_GCM_NONCE_SIZE 12
#define HYBBX_BACKEND_AES_GCM_TAG_SIZE 16
#define HYBBX_BACKEND_XCHACHA_NONCE_SIZE 24
#define HYBBX_BACKEND_TAG_SIZE 16

void hybbx_backend_sha256_hex(const char *data, size_t len, char hex[65]);

int hybbx_backend_aes256gcm_encrypt(
    const uint8_t key[HYBBX_BACKEND_AES_GCM_KEY_SIZE],
    const uint8_t nonce[HYBBX_BACKEND_AES_GCM_NONCE_SIZE],
    const uint8_t *aad, size_t aad_len, const uint8_t *plaintext,
    size_t plaintext_len, uint8_t *ciphertext,
    uint8_t tag[HYBBX_BACKEND_AES_GCM_TAG_SIZE]);

int hybbx_backend_aes256gcm_decrypt(
    const uint8_t key[HYBBX_BACKEND_AES_GCM_KEY_SIZE],
    const uint8_t nonce[HYBBX_BACKEND_AES_GCM_NONCE_SIZE],
    const uint8_t *aad, size_t aad_len, const uint8_t *ciphertext,
    size_t ciphertext_len, uint8_t *plaintext,
    const uint8_t tag[HYBBX_BACKEND_AES_GCM_TAG_SIZE]);

void hybbx_backend_chacha_lock(uint8_t *ciphertext, uint8_t tag[16],
                             const uint8_t key[32],
                             const uint8_t nonce[24], const uint8_t *aad,
                             size_t aad_len, const uint8_t *plaintext,
                             size_t plaintext_len);

int hybbx_backend_chacha_unlock(uint8_t *plaintext, const uint8_t tag[16],
                              const uint8_t key[32], const uint8_t nonce[24],
                              const uint8_t *aad, size_t aad_len,
                              const uint8_t *ciphertext, size_t ciphertext_len);

void hybbx_backend_x25519_public_key(uint8_t public_key[32],
                                     const uint8_t secret_key[32]);
void hybbx_backend_x25519_shared(uint8_t shared[32],
                                 const uint8_t secret_key[32],
                                 const uint8_t peer_public_key[32]);
void hybbx_backend_blake2b_keyed(uint8_t *hash, size_t hash_size,
                                 const uint8_t *message, size_t message_size,
                                 const uint8_t *key, size_t key_size);

hybbx_result_t hybbx_backend_random(uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CRYPTO_BACKENDS_H */
