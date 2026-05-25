#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>

/* Compress data. Returns compressed size, or -1 on failure.
 * dst must be at least compress_bound(src_len) bytes. */
int compress_data(const void *src, size_t src_len, void *dst, size_t dst_len);

/* Decompress data. Returns decompressed size, or -1 on failure.
 * dst must be large enough to hold decompressed data. */
int decompress_data(const void *src, size_t src_len, void *dst, size_t dst_len);

/* Returns maximum compressed size for given input size */
size_t compress_bound_size(size_t src_len);

#endif
