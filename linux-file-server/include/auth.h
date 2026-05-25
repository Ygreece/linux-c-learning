#ifndef AUTH_H
#define AUTH_H

#include "common.h"
#include <openssl/evp.h>
#include <openssl/sha.h>

#define AUTH_MAX_USERS 64
#define AUTH_TOKEN_LEN 64
#define AUTH_SALT_LEN  16

typedef struct {
    char username[32];
    char password_hash[128];  /* SHA-256 hex = 64 chars */
    char salt[AUTH_SALT_LEN + 1];
    int permissions;           /* r=1, w=2, rw=3 */
} auth_user_t;

typedef struct {
    auth_user_t users[AUTH_MAX_USERS];
    int user_count;
    char token_store[AUTH_MAX_USERS][AUTH_TOKEN_LEN]; /* simple token storage */
} auth_context_t;

int auth_init(auth_context_t *ctx, const char *users_file);
int auth_login(auth_context_t *ctx, const char *username, const char *password, char *token_out);
int auth_verify(const auth_context_t *ctx, const char *token, int *permissions_out);
int auth_hash_password(const char *password, const char *salt, char *hex_out, size_t hex_size);
void auth_generate_salt(char *salt_out, size_t len);
void auth_generate_token(char *token_out, size_t len);

#endif
