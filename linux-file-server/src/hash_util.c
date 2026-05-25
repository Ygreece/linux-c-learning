#include "hash_util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

#define CHUNK_SIZE 4096

/* Convert binary digest to lowercase hex string */
static void digest_to_hex(const unsigned char *digest, size_t digest_len,
                          char *hex_out, size_t hex_size)
{
    static const char hex_chars[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < digest_len && (i * 2 + 2) < hex_size; i++) {
        hex_out[i * 2]     = hex_chars[(digest[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[digest[i] & 0x0F];
    }
    hex_out[i * 2] = '\0';
}

/* Internal: hash data with the given EVP_MD */
static int hash_data(const void *data, size_t len, const EVP_MD *md,
                     char *hex_out, size_t hex_size)
{
    EVP_MD_CTX *ctx = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (!data || !hex_out) {
        return -1;
    }

    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    digest_to_hex(digest, digest_len, hex_out, hex_size);
    return 0;
}

/* Internal: hash a file with the given EVP_MD */
static int hash_file(const char *filepath, const EVP_MD *md,
                     char *hex_out, size_t hex_size)
{
    FILE *fp = NULL;
    EVP_MD_CTX *ctx = NULL;
    unsigned char buf[CHUNK_SIZE];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    size_t n;

    if (!filepath || !hex_out) {
        return -1;
    }

    fp = fopen(filepath, "rb");
    if (!fp) {
        return -1;
    }

    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(fp);
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    while ((n = fread(buf, 1, CHUNK_SIZE, fp)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    fclose(fp);
    digest_to_hex(digest, digest_len, hex_out, hex_size);
    return 0;
}

int md5_file(const char *filepath, char *hex_out, size_t hex_size)
{
    return hash_file(filepath, EVP_md5(), hex_out, hex_size);
}

int md5_data(const void *data, size_t len, char *hex_out, size_t hex_size)
{
    return hash_data(data, len, EVP_md5(), hex_out, hex_size);
}

int sha256_file(const char *filepath, char *hex_out, size_t hex_size)
{
    return hash_file(filepath, EVP_sha256(), hex_out, hex_size);
}

int sha256_data(const void *data, size_t len, char *hex_out, size_t hex_size)
{
    return hash_data(data, len, EVP_sha256(), hex_out, hex_size);
}
