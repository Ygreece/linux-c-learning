#include "hash_util.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void test_md5_data(void)
{
    char hex[33];
    assert(md5_data("hello", 5, hex, sizeof(hex)) == 0);
    /* MD5("hello") = 5d41402abc4b2a76b9719d911017c592 */
    assert(strcmp(hex, "5d41402abc4b2a76b9719d911017c592") == 0);
}

void test_md5_empty(void)
{
    char hex[33];
    assert(md5_data("", 0, hex, sizeof(hex)) == 0);
    /* MD5("") = d41d8cd98f00b204e9800998ecf8427e */
    assert(strcmp(hex, "d41d8cd98f00b204e9800998ecf8427e") == 0);
}

void test_sha256_data(void)
{
    char hex[65];
    assert(sha256_data("hello", 5, hex, sizeof(hex)) == 0);
    /* SHA256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824 */
    assert(strlen(hex) == 64);
}

void test_md5_file(void)
{
    /* Create a temp file */
    FILE *f = fopen("/tmp/test_hash.txt", "w");
    fprintf(f, "hello");
    fclose(f);

    char hex[33];
    assert(md5_file("/tmp/test_hash.txt", hex, sizeof(hex)) == 0);
    assert(strcmp(hex, "5d41402abc4b2a76b9719d911017c592") == 0);

    unlink("/tmp/test_hash.txt");
}

void test_sha256_file(void)
{
    FILE *f = fopen("/tmp/test_hash2.txt", "w");
    fprintf(f, "hello");
    fclose(f);

    char hex[65];
    assert(sha256_file("/tmp/test_hash2.txt", hex, sizeof(hex)) == 0);
    assert(strlen(hex) == 64);

    unlink("/tmp/test_hash2.txt");
}

void test_consistency(void)
{
    char hex1[33], hex2[33];
    md5_data("test", 4, hex1, sizeof(hex1));
    md5_data("test", 4, hex2, sizeof(hex2));
    assert(strcmp(hex1, hex2) == 0);
}

int main(void)
{
    test_md5_data();
    test_md5_empty();
    test_sha256_data();
    test_md5_file();
    test_sha256_file();
    test_consistency();
    printf("All hash tests passed!\n");
    return 0;
}
