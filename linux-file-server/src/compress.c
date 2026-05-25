#include "compress.h"
#include <zlib.h>
#include <string.h>

int compress_data(const void *src, size_t src_len, void *dst, size_t dst_len) {
    uLongf comp_len = dst_len;
    int ret = compress2(dst, &comp_len, src, src_len, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) return -1;
    return (int)comp_len;
}

int decompress_data(const void *src, size_t src_len, void *dst, size_t dst_len) {
    uLongf decomp_len = dst_len;
    int ret = uncompress(dst, &decomp_len, src, src_len);
    if (ret != Z_OK) return -1;
    return (int)decomp_len;
}

size_t compress_bound_size(size_t src_len) {
    return compressBound(src_len);
}
