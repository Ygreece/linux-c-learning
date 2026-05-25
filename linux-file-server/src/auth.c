#include "auth.h"
#include <ctype.h>

/* Convert binary SHA-256 digest to hex string */
static void digest_to_hex(const unsigned char *digest, char *hex_out, size_t hex_size)
{
    size_t i;
    size_t count = SHA256_DIGEST_LENGTH;
    if (hex_size < count * 2 + 1) {
        count = (hex_size - 1) / 2;
    }
    for (i = 0; i < count; i++) {
        snprintf(hex_out + i * 2, 3, "%02x", digest[i]);
    }
    hex_out[count * 2] = '\0';
}

/* Read random bytes from /dev/urandom */
static int read_urandom(unsigned char *buf, size_t len)
{
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        return -1;
    }
    if (fread(buf, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int auth_hash_password(const char *password, const char *salt, char *hex_out, size_t hex_size)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digest_len = 0;
    EVP_MD_CTX *mdctx;
    size_t salt_len;
    size_t pass_len;

    if (!password || !salt || !hex_out || hex_size < SHA256_DIGEST_LENGTH * 2 + 1) {
        return -1;
    }

    salt_len = strlen(salt);
    pass_len = strlen(password);

    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return -1;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    if (EVP_DigestUpdate(mdctx, salt, salt_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    if (EVP_DigestUpdate(mdctx, password, pass_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }

    EVP_MD_CTX_free(mdctx);
    digest_to_hex(digest, hex_out, hex_size);
    return 0;
}

void auth_generate_salt(char *salt_out, size_t len)
{
    unsigned char buf[AUTH_SALT_LEN];
    size_t i;

    if (len > AUTH_SALT_LEN) {
        len = AUTH_SALT_LEN;
    }

    if (read_urandom(buf, len) != 0) {
        /* Fallback: use time-based seed (less secure) */
        srand((unsigned int)time(NULL));
        for (i = 0; i < len; i++) {
            buf[i] = (unsigned char)(rand() % 256);
        }
    }

    for (i = 0; i < len; i++) {
        snprintf(salt_out + i * 2, 3, "%02x", buf[i]);
    }
    salt_out[len * 2] = '\0';
}

void auth_generate_token(char *token_out, size_t len)
{
    unsigned char buf[AUTH_TOKEN_LEN / 2 + 1];
    size_t byte_count;
    size_t i;

    if (len > AUTH_TOKEN_LEN) {
        len = AUTH_TOKEN_LEN;
    }

    byte_count = (len + 1) / 2;

    if (read_urandom(buf, byte_count) != 0) {
        srand((unsigned int)time(NULL));
        for (i = 0; i < byte_count; i++) {
            buf[i] = (unsigned char)(rand() % 256);
        }
    }

    for (i = 0; i < byte_count && i * 2 < len; i++) {
        snprintf(token_out + i * 2, 3, "%02x", buf[i]);
    }
    token_out[len] = '\0';
}

int auth_init(auth_context_t *ctx, const char *users_file)
{
    FILE *fp;
    char line[512];
    char *tok;
    char *saveptr;

    if (!ctx || !users_file) {
        return -1;
    }

    memset(ctx, 0, sizeof(auth_context_t));
    ctx->user_count = 0;

    fp = fopen(users_file, "r");
    if (!fp) {
        fprintf(stderr, "auth_init: cannot open %s: %s\n", users_file, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        auth_user_t *user;
        char *p;

        /* Trim trailing newline/carriage return */
        p = line + strlen(line) - 1;
        while (p >= line && (*p == '\n' || *p == '\r')) {
            *p-- = '\0';
        }

        /* Skip blank lines and comments */
        p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }

        if (ctx->user_count >= AUTH_MAX_USERS) {
            fprintf(stderr, "auth_init: max users reached\n");
            break;
        }

        user = &ctx->users[ctx->user_count];

        /* Parse: username:hash:salt:permissions */
        tok = strtok_r(p, ":", &saveptr);
        if (!tok) continue;
        snprintf(user->username, sizeof(user->username), "%s", tok);

        tok = strtok_r(NULL, ":", &saveptr);
        if (!tok) continue;
        snprintf(user->password_hash, sizeof(user->password_hash), "%s", tok);

        tok = strtok_r(NULL, ":", &saveptr);
        if (!tok) continue;
        snprintf(user->salt, sizeof(user->salt), "%s", tok);

        tok = strtok_r(NULL, ":", &saveptr);
        if (!tok) continue;
        user->permissions = atoi(tok);

        ctx->user_count++;
    }

    fclose(fp);
    return 0;
}

int auth_login(auth_context_t *ctx, const char *username, const char *password, char *token_out)
{
    char computed_hash[128];
    int i;

    if (!ctx || !username || !password || !token_out) {
        return -1;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) != 0) {
            continue;
        }

        /* Found user - verify password */
        if (auth_hash_password(password, ctx->users[i].salt,
                               computed_hash, sizeof(computed_hash)) != 0) {
            return -1;
        }

        if (strcmp(computed_hash, ctx->users[i].password_hash) != 0) {
            return -1; /* Password mismatch */
        }

        /* Generate and store token */
        auth_generate_token(ctx->token_store[i], AUTH_TOKEN_LEN);
        memcpy(token_out, ctx->token_store[i], AUTH_TOKEN_LEN);
        token_out[AUTH_TOKEN_LEN] = '\0';

        return 0;
    }

    return -1; /* User not found */
}

int auth_verify(const auth_context_t *ctx, const char *token, int *permissions_out)
{
    int i;

    if (!ctx || !token || !permissions_out) {
        return -1;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->token_store[i], token) == 0) {
            *permissions_out = ctx->users[i].permissions;
            return 0;
        }
    }

    return -1; /* Token not found */
}
