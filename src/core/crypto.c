#include "hybbx/crypto.h"

#include "crypto_backends.h"
#include "monocypher.h"

#include <string.h>

#define HYBBX_CRYPTO_MAX_BYTES (256u * 1024u)

static hybbx_result_t crypto_len_ok(size_t len)
{
    if (len > HYBBX_CRYPTO_MAX_BYTES) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_crypto_random(uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_backend_random(buf, len);
}

const char *hybbx_crypto_alg_name(hybbx_crypto_alg_t alg)
{
    switch (alg) {
    case HYBBX_CRYPTO_AES_256_GCM:
        return "aes-256-gcm";
    case HYBBX_CRYPTO_XCHACHA20_POLY1305:
        return "xchacha20-poly1305";
    case HYBBX_CRYPTO_X25519_AEAD:
        return "x25519-xchacha20-poly1305";
    default:
        return "unknown";
    }
}

size_t hybbx_crypto_nonce_size(hybbx_crypto_alg_t alg)
{
    switch (alg) {
    case HYBBX_CRYPTO_AES_256_GCM:
        return HYBBX_CRYPTO_AES_GCM_NONCE;
    case HYBBX_CRYPTO_XCHACHA20_POLY1305:
        return HYBBX_CRYPTO_XCHACHA_NONCE;
    default:
        return 0;
    }
}

static hybbx_result_t derive_x25519_key(const uint8_t shared[32],
                                        uint8_t key[32])
{
    static const uint8_t label[] = "hybbx-x25519-aead-key-v1";

    hybbx_backend_blake2b_keyed(key, 32, shared, 32, label, sizeof(label) - 1);
    return HYBBX_OK;
}

hybbx_result_t hybbx_crypto_encrypt(hybbx_crypto_alg_t alg,
                                    const uint8_t key[HYBBX_CRYPTO_KEY_SIZE],
                                    const uint8_t *nonce, size_t nonce_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *plaintext,
                                    size_t plaintext_len,
                                    uint8_t *ciphertext,
                                    uint8_t tag[HYBBX_CRYPTO_TAG_SIZE])
{
    hybbx_result_t rc;

    if (key == NULL || nonce == NULL || tag == NULL ||
        (plaintext_len > 0 && (plaintext == NULL || ciphertext == NULL))) {
        return HYBBX_ERR_INVALID;
    }

    rc = crypto_len_ok(plaintext_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (alg == HYBBX_CRYPTO_AES_256_GCM) {
        if (nonce_len != HYBBX_CRYPTO_AES_GCM_NONCE) {
            return HYBBX_ERR_INVALID;
        }
        if (hybbx_backend_aes256gcm_encrypt(key, nonce, aad, aad_len, plaintext,
                                            plaintext_len, ciphertext, tag) !=
            0) {
            return HYBBX_ERR_IO;
        }
        return HYBBX_OK;
    }

    if (alg == HYBBX_CRYPTO_XCHACHA20_POLY1305) {
        uint8_t nonce24[HYBBX_CRYPTO_XCHACHA_NONCE];

        if (nonce_len != HYBBX_CRYPTO_XCHACHA_NONCE) {
            return HYBBX_ERR_INVALID;
        }
        memcpy(nonce24, nonce, sizeof(nonce24));
        hybbx_backend_chacha_lock(ciphertext, tag, key, nonce24, aad, aad_len,
                                  plaintext, plaintext_len);
        crypto_wipe(nonce24, sizeof(nonce24));
        return HYBBX_OK;
    }

    return HYBBX_ERR_INVALID;
}

hybbx_result_t hybbx_crypto_decrypt(hybbx_crypto_alg_t alg,
                                    const uint8_t key[HYBBX_CRYPTO_KEY_SIZE],
                                    const uint8_t *nonce, size_t nonce_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *ciphertext,
                                    size_t ciphertext_len,
                                    uint8_t *plaintext,
                                    const uint8_t tag[HYBBX_CRYPTO_TAG_SIZE])
{
    hybbx_result_t rc;
    int ok;

    if (key == NULL || nonce == NULL || tag == NULL ||
        (ciphertext_len > 0 && (ciphertext == NULL || plaintext == NULL))) {
        return HYBBX_ERR_INVALID;
    }

    rc = crypto_len_ok(ciphertext_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (alg == HYBBX_CRYPTO_AES_256_GCM) {
        if (nonce_len != HYBBX_CRYPTO_AES_GCM_NONCE) {
            return HYBBX_ERR_INVALID;
        }
        ok = hybbx_backend_aes256gcm_decrypt(key, nonce, aad, aad_len,
                                           ciphertext, ciphertext_len,
                                           plaintext, tag);
        return ok == 0 ? HYBBX_OK : HYBBX_ERR_DENIED;
    }

    if (alg == HYBBX_CRYPTO_XCHACHA20_POLY1305) {
        uint8_t nonce24[HYBBX_CRYPTO_XCHACHA_NONCE];

        if (nonce_len != HYBBX_CRYPTO_XCHACHA_NONCE) {
            return HYBBX_ERR_INVALID;
        }
        memcpy(nonce24, nonce, sizeof(nonce24));
        ok = hybbx_backend_chacha_unlock(plaintext, tag, key, nonce24, aad,
                                       aad_len, ciphertext, ciphertext_len);
        crypto_wipe(nonce24, sizeof(nonce24));
        return ok == 0 ? HYBBX_OK : HYBBX_ERR_DENIED;
    }

    return HYBBX_ERR_INVALID;
}

hybbx_result_t hybbx_crypto_x25519_keypair(
    uint8_t public_key[HYBBX_CRYPTO_X25519_PUBLIC_KEY],
    uint8_t secret_key[HYBBX_CRYPTO_X25519_SECRET_KEY])
{
    if (public_key == NULL || secret_key == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_crypto_random(secret_key, HYBBX_CRYPTO_X25519_SECRET_KEY) !=
        HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    secret_key[0] &= 248;
    secret_key[31] &= 127;
    secret_key[31] |= 64;

    hybbx_backend_x25519_public_key(public_key, secret_key);
    return HYBBX_OK;
}

hybbx_result_t hybbx_crypto_x25519_seal(
    const uint8_t recipient_pk[HYBBX_CRYPTO_X25519_PUBLIC_KEY],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *sealed, size_t *sealed_len, size_t sealed_cap)
{
    uint8_t ep_sk[32];
    uint8_t ep_pk[32];
    uint8_t shared[32];
    uint8_t key[32];
    uint8_t nonce[HYBBX_CRYPTO_XCHACHA_NONCE];
    uint8_t *cipher;
    uint8_t *tag;
    size_t need;
    hybbx_result_t rc;

    if (recipient_pk == NULL || sealed == NULL || sealed_len == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = crypto_len_ok(plaintext_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    need = HYBBX_CRYPTO_X25519_SEAL_OVERHEAD + plaintext_len;
    if (sealed_cap < need) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_crypto_random(ep_sk, sizeof(ep_sk)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }
    ep_sk[0] &= 248;
    ep_sk[31] &= 127;
    ep_sk[31] |= 64;

    hybbx_backend_x25519_public_key(ep_pk, ep_sk);
    hybbx_backend_x25519_shared(shared, ep_sk, recipient_pk);
    derive_x25519_key(shared, key);

    if (hybbx_crypto_random(nonce, sizeof(nonce)) != HYBBX_OK) {
        crypto_wipe(ep_sk, sizeof(ep_sk));
        crypto_wipe(shared, sizeof(shared));
        crypto_wipe(key, sizeof(key));
        return HYBBX_ERR_IO;
    }

    memcpy(sealed, ep_pk, 32);
    memcpy(sealed + 32, nonce, sizeof(nonce));
    cipher = sealed + 32 + sizeof(nonce);
    tag = sealed + 32 + sizeof(nonce) + plaintext_len;

    rc = hybbx_crypto_encrypt(HYBBX_CRYPTO_XCHACHA20_POLY1305, key, nonce,
                              sizeof(nonce), aad, aad_len, plaintext,
                              plaintext_len, cipher, tag);

    crypto_wipe(ep_sk, sizeof(ep_sk));
    crypto_wipe(shared, sizeof(shared));
    crypto_wipe(key, sizeof(key));

    if (rc != HYBBX_OK) {
        return rc;
    }

    *sealed_len = need;
    return HYBBX_OK;
}

hybbx_result_t hybbx_crypto_x25519_open(
    const uint8_t recipient_sk[HYBBX_CRYPTO_X25519_SECRET_KEY],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *sealed, size_t sealed_len,
    uint8_t *plaintext, size_t *plaintext_len, size_t plaintext_cap)
{
    const uint8_t *ep_pk;
    const uint8_t *nonce;
    const uint8_t *cipher;
    const uint8_t *tag;
    size_t cipher_len;
    uint8_t shared[32];
    uint8_t key[32];
    uint8_t tag_buf[HYBBX_CRYPTO_TAG_SIZE];
    hybbx_result_t rc;

    if (recipient_sk == NULL || sealed == NULL || plaintext == NULL ||
        plaintext_len == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (sealed_len < HYBBX_CRYPTO_X25519_SEAL_OVERHEAD) {
        return HYBBX_ERR_INVALID;
    }

    cipher_len = sealed_len - HYBBX_CRYPTO_X25519_SEAL_OVERHEAD;
    if (cipher_len > plaintext_cap) {
        return HYBBX_ERR_INVALID;
    }

    ep_pk = sealed;
    nonce = sealed + 32;
    cipher = sealed + 32 + HYBBX_CRYPTO_XCHACHA_NONCE;
    tag = cipher + cipher_len;
    memcpy(tag_buf, tag, sizeof(tag_buf));

    hybbx_backend_x25519_shared(shared, recipient_sk, ep_pk);
    derive_x25519_key(shared, key);

    rc = hybbx_crypto_decrypt(HYBBX_CRYPTO_XCHACHA20_POLY1305, key, nonce,
                              HYBBX_CRYPTO_XCHACHA_NONCE, aad, aad_len,
                              cipher, cipher_len, plaintext, tag_buf);

    crypto_wipe(shared, sizeof(shared));
    crypto_wipe(key, sizeof(key));

    if (rc != HYBBX_OK) {
        return rc;
    }

    *plaintext_len = cipher_len;
    return HYBBX_OK;
}
