#ifndef HASH_UTIL_H
#define HASH_UTIL_H

#include <stddef.h>

/* Calculate MD5 of a file, output as hex string (32 chars + null) */
int md5_file(const char *filepath, char *hex_out, size_t hex_size);

/* Calculate MD5 of memory data */
int md5_data(const void *data, size_t len, char *hex_out, size_t hex_size);

/* Calculate SHA-256 of a file, output as hex string (64 chars + null) */
int sha256_file(const char *filepath, char *hex_out, size_t hex_size);

/* Calculate SHA-256 of memory data */
int sha256_data(const void *data, size_t len, char *hex_out, size_t hex_size);

#endif
