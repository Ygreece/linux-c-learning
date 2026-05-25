#include "auth.h"
#include <assert.h>

void test_hash_password(void)
{
    char hash[128];

    assert(auth_hash_password("test123", "salt1", hash, sizeof(hash)) == 0);
    assert(strlen(hash) == 64); /* SHA-256 hex = 64 chars */

    /* Same input should give same hash */
    char hash2[128];
    assert(auth_hash_password("test123", "salt1", hash2, sizeof(hash2)) == 0);
    assert(strcmp(hash, hash2) == 0);

    /* Different salt should give different hash */
    char hash3[128];
    assert(auth_hash_password("test123", "salt2", hash3, sizeof(hash3)) == 0);
    assert(strcmp(hash, hash3) != 0);

    printf("  [PASS] test_hash_password\n");
}

void test_salt_generation(void)
{
    char salt1[AUTH_SALT_LEN * 2 + 1], salt2[AUTH_SALT_LEN * 2 + 1];

    auth_generate_salt(salt1, AUTH_SALT_LEN);
    auth_generate_salt(salt2, AUTH_SALT_LEN);

    assert(strlen(salt1) == AUTH_SALT_LEN * 2);
    assert(strcmp(salt1, salt2) != 0); /* Should be different */

    printf("  [PASS] test_salt_generation\n");
}

void test_token_generation(void)
{
    char tok1[AUTH_TOKEN_LEN + 1], tok2[AUTH_TOKEN_LEN + 1];

    auth_generate_token(tok1, AUTH_TOKEN_LEN);
    auth_generate_token(tok2, AUTH_TOKEN_LEN);

    assert(strlen(tok1) == AUTH_TOKEN_LEN);
    assert(strcmp(tok1, tok2) != 0);

    printf("  [PASS] test_token_generation\n");
}

void test_auth_login_verify(void)
{
    auth_context_t ctx;
    char token[AUTH_TOKEN_LEN + 1];
    int perms = 0;

    assert(auth_init(&ctx, "config/users.conf") == 0);
    assert(ctx.user_count == 2);

    /* Admin login with correct password */
    memset(token, 0, sizeof(token));
    assert(auth_login(&ctx, "admin", "admin123", token) == 0);
    assert(strlen(token) == AUTH_TOKEN_LEN);

    /* Verify admin token gives rw=3 permissions */
    assert(auth_verify(&ctx, token, &perms) == 0);
    assert(perms == 3);

    /* Admin login with wrong password should fail */
    assert(auth_login(&ctx, "admin", "wrongpass", token) != 0);

    /* Guest login */
    memset(token, 0, sizeof(token));
    assert(auth_login(&ctx, "guest", "guest123", token) == 0);
    assert(auth_verify(&ctx, token, &perms) == 0);
    assert(perms == 1);

    /* Unknown user should fail */
    assert(auth_login(&ctx, "nobody", "pass", token) != 0);

    /* Invalid token should fail */
    assert(auth_verify(&ctx, "invalidtoken", &perms) != 0);

    printf("  [PASS] test_auth_login_verify\n");
}

int main(void)
{
    printf("Running auth tests...\n");
    test_hash_password();
    test_salt_generation();
    test_token_generation();
    test_auth_login_verify();
    printf("All auth tests passed!\n");
    return 0;
}
