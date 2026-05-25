#include "compress.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_compress_decompress(void) {
    const char *original = "Hello World! This is a test string that should compress well "
                           "because it has repeated patterns repeated patterns repeated patterns "
                           "repeated patterns repeated patterns repeated patterns.";
    size_t orig_len = strlen(original);

    size_t comp_buf_size = compress_bound_size(orig_len);
    char *compressed = malloc(comp_buf_size);
    char *decompressed = malloc(orig_len + 1);

    int comp_len = compress_data(original, orig_len, compressed, comp_buf_size);
    assert(comp_len > 0);
    assert((size_t)comp_len < orig_len);  /* Should be smaller */

    int decomp_len = decompress_data(compressed, comp_len, decompressed, orig_len + 1);
    assert(decomp_len == (int)orig_len);
    decompressed[decomp_len] = '\0';
    assert(strcmp(decompressed, original) == 0);

    free(compressed);
    free(decompressed);
}

void test_compress_empty(void) {
    char compressed[256];
    int ret = compress_data("", 0, compressed, sizeof(compressed));
    assert(ret >= 0);
}

void test_compress_large(void) {
    /* Create a 100KB repetitive buffer */
    size_t size = 100 * 1024;
    char *data = malloc(size);
    for (size_t i = 0; i < size; i++) {
        data[i] = 'A' + (i % 26);
    }

    size_t comp_size = compress_bound_size(size);
    char *compressed = malloc(comp_size);

    int comp_len = compress_data(data, size, compressed, comp_size);
    assert(comp_len > 0);
    assert((size_t)comp_len < size / 2);  /* Repetitive data should compress well */

    char *decompressed = malloc(size);
    int decomp_len = decompress_data(compressed, comp_len, decompressed, size);
    assert(decomp_len == (int)size);
    assert(memcmp(decompressed, data, size) == 0);

    free(data);
    free(compressed);
    free(decompressed);
}

int main(void) {
    test_compress_decompress();
    test_compress_empty();
    test_compress_large();
    printf("All compression tests passed!\n");
    return 0;
}
