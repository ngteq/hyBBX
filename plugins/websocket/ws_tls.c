#include "ws_tls.h"

#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/websocket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HYBBX_WS_HAVE_TLS

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

static SSL_CTX *g_ssl_ctx;
static int g_tls_enabled;

static hybbx_result_t mkdir_cert_dir(const char *cert_dir)
{
    char parent[HYBBX_PATH_MAX];
    struct stat st;

    if (cert_dir == NULL || cert_dir[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (stat(cert_dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? HYBBX_OK : HYBBX_ERR_IO;
    }

    if (hybbx_path_dirname(cert_dir, parent, sizeof(parent)) == HYBBX_OK &&
        parent[0] != '\0' && strcmp(parent, cert_dir) != 0 &&
        stat(parent, &st) != 0) {
        if (mkdir_cert_dir(parent) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
    }

    if (mkdir(cert_dir, 0700) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static void build_cert_path(const char *cert_dir, const char *name,
                            char *out, size_t out_len)
{
    if (cert_dir == NULL || name == NULL || out == NULL || out_len == 0) {
        if (out != NULL && out_len > 0) {
            out[0] = '\0';
        }
        return;
    }

    snprintf(out, out_len, "%s/%s", cert_dir, name);
}

static int cert_files_exist(const char *cert_path, const char *key_path)
{
    struct stat st;

    return stat(cert_path, &st) == 0 && S_ISREG(st.st_mode) &&
           stat(key_path, &st) == 0 && S_ISREG(st.st_mode);
}

static hybbx_result_t generate_self_signed(const char *cert_path,
                                           const char *key_path)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    X509 *cert = NULL;
    X509_NAME *name;
    FILE *fp;
    int rc = HYBBX_ERR_IO;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (pctx == NULL ||
        EVP_PKEY_keygen_init(pctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0 ||
        EVP_PKEY_keygen(pctx, &pkey) <= 0 || pkey == NULL) {
        goto done;
    }

    cert = X509_new();
    if (cert == NULL) {
        goto done;
    }

    if (X509_set_version(cert, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) != 1 ||
        X509_gmtime_adj(X509_get_notBefore(cert), 0) == NULL ||
        X509_gmtime_adj(X509_get_notAfter(cert),
                        (long)HYBBX_WS_TLS_CERT_VALID_DAYS * 24L * 3600L) ==
                NULL ||
        X509_set_pubkey(cert, pkey) != 1) {
        goto done;
    }

    name = X509_get_subject_name(cert);
    if (name == NULL ||
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const unsigned char *)"hybbx", -1, -1,
                                   0) != 1 ||
        X509_set_issuer_name(cert, name) != 1 ||
        X509_sign(cert, pkey, EVP_sha256()) <= 0) {
        goto done;
    }

    fp = fopen(key_path, "w");
    if (fp == NULL ||
        PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL) != 1) {
        if (fp != NULL) {
            fclose(fp);
            unlink(key_path);
        }
        goto done;
    }
    fclose(fp);
    (void)chmod(key_path, 0600);

    fp = fopen(cert_path, "w");
    if (fp == NULL || PEM_write_X509(fp, cert) != 1) {
        if (fp != NULL) {
            fclose(fp);
            unlink(cert_path);
        }
        goto done;
    }
    fclose(fp);
    (void)chmod(cert_path, 0644);

    rc = HYBBX_OK;

done:
    if (cert != NULL) {
        X509_free(cert);
    }
    if (pkey != NULL) {
        EVP_PKEY_free(pkey);
    }
    if (pctx != NULL) {
        EVP_PKEY_CTX_free(pctx);
    }

    return rc;
}

int hybbx_ws_tls_compiled(void)
{
    return 1;
}

hybbx_result_t hybbx_ws_tls_ensure_certs(const char *cert_dir)
{
    char cert_path[HYBBX_PATH_MAX];
    char key_path[HYBBX_PATH_MAX];
    hybbx_result_t rc;

    if (cert_dir == NULL || cert_dir[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    rc = mkdir_cert_dir(cert_dir);
    if (rc != HYBBX_OK) {
        return rc;
    }

    build_cert_path(cert_dir, HYBBX_WS_TLS_CERT_FILENAME, cert_path,
                    sizeof(cert_path));
    build_cert_path(cert_dir, HYBBX_WS_TLS_KEY_FILENAME, key_path,
                    sizeof(key_path));

    if (cert_files_exist(cert_path, key_path)) {
        return HYBBX_OK;
    }

    rc = generate_self_signed(cert_path, key_path);
    if (rc == HYBBX_OK) {
        printf("[websocket] created self-signed TLS certificate in %s\n",
               cert_dir);
    }

    return rc;
}

hybbx_result_t hybbx_ws_tls_server_start(const char *cert_dir)
{
    char cert_path[HYBBX_PATH_MAX];
    char key_path[HYBBX_PATH_MAX];

    if (g_tls_enabled) {
        return HYBBX_OK;
    }

    if (cert_dir == NULL || cert_dir[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    build_cert_path(cert_dir, HYBBX_WS_TLS_CERT_FILENAME, cert_path,
                    sizeof(cert_path));
    build_cert_path(cert_dir, HYBBX_WS_TLS_KEY_FILENAME, key_path,
                    sizeof(key_path));

    if (!cert_files_exist(cert_path, key_path)) {
        return HYBBX_ERR_IO;
    }

    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                             OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                         NULL) != 1) {
        return HYBBX_ERR_IO;
    }

    g_ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (g_ssl_ctx == NULL) {
        return HYBBX_ERR_IO;
    }

    SSL_CTX_set_min_proto_version(g_ssl_ctx, TLS1_2_VERSION);

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, cert_path, SSL_FILETYPE_PEM) !=
            1 ||
        SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key_path, SSL_FILETYPE_PEM) !=
            1 ||
        SSL_CTX_check_private_key(g_ssl_ctx) != 1) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
        return HYBBX_ERR_IO;
    }

    g_tls_enabled = 1;
    return HYBBX_OK;
}

void hybbx_ws_tls_server_stop(void)
{
    if (g_ssl_ctx != NULL) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
    g_tls_enabled = 0;
}

int hybbx_ws_tls_server_enabled(void)
{
    return g_tls_enabled;
}

void *hybbx_ws_tls_accept(int fd)
{
    SSL *ssl;

    if (!g_tls_enabled || g_ssl_ctx == NULL || fd < 0) {
        return NULL;
    }

    ssl = SSL_new(g_ssl_ctx);
    if (ssl == NULL) {
        return NULL;
    }

    if (SSL_set_fd(ssl, fd) != 1 || SSL_accept(ssl) <= 0) {
        SSL_free(ssl);
        return NULL;
    }

    return ssl;
}

ssize_t hybbx_ws_tls_recv(void *tls, void *buf, size_t len)
{
    SSL *ssl = (SSL *)tls;
    int n;

    if (ssl == NULL || buf == NULL || len == 0) {
        return -1;
    }

    do {
        n = SSL_read(ssl, buf, (int)len);
    } while (n < 0 && SSL_get_error(ssl, n) == SSL_ERROR_SYSCALL &&
             errno == EINTR);

    return (ssize_t)n;
}

ssize_t hybbx_ws_tls_send(void *tls, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    SSL *ssl = (SSL *)tls;
    size_t sent = 0;

    if (ssl == NULL || buf == NULL) {
        return -1;
    }

    while (sent < len) {
        int n;

        do {
            n = SSL_write(ssl, p + sent, (int)(len - sent));
        } while (n < 0 && SSL_get_error(ssl, n) == SSL_ERROR_SYSCALL &&
                 errno == EINTR);

        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return (ssize_t)sent;
}

void hybbx_ws_tls_shutdown(void *tls)
{
    SSL *ssl = (SSL *)tls;

    if (ssl != NULL) {
        (void)SSL_shutdown(ssl);
    }
}

void hybbx_ws_tls_free(void *tls)
{
    SSL *ssl = (SSL *)tls;

    if (ssl != NULL) {
        SSL_free(ssl);
    }
}

#else /* !HYBBX_WS_HAVE_TLS */

int hybbx_ws_tls_compiled(void)
{
    return 0;
}

hybbx_result_t hybbx_ws_tls_ensure_certs(const char *cert_dir)
{
    (void)cert_dir;
    return HYBBX_OK;
}

hybbx_result_t hybbx_ws_tls_server_start(const char *cert_dir)
{
    (void)cert_dir;
    return HYBBX_ERR_UNSUPPORTED;
}

void hybbx_ws_tls_server_stop(void)
{
}

int hybbx_ws_tls_server_enabled(void)
{
    return 0;
}

void *hybbx_ws_tls_accept(int fd)
{
    (void)fd;
    return NULL;
}

ssize_t hybbx_ws_tls_recv(void *tls, void *buf, size_t len)
{
    (void)tls;
    (void)buf;
    (void)len;
    return -1;
}

ssize_t hybbx_ws_tls_send(void *tls, const void *buf, size_t len)
{
    (void)tls;
    (void)buf;
    (void)len;
    return -1;
}

void hybbx_ws_tls_shutdown(void *tls)
{
    (void)tls;
}

void hybbx_ws_tls_free(void *tls)
{
    (void)tls;
}

#endif /* HYBBX_WS_HAVE_TLS */
