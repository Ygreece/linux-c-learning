#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>

typedef struct mempool mempool_t;

/* Create a memory pool with blocks of block_size bytes, count blocks total.
 * Returns NULL on failure. */
mempool_t *mempool_create(size_t block_size, size_t count);

/* Allocate one block from the pool. Returns NULL if pool exhausted.
 * Thread-safe. */
void *mempool_alloc(mempool_t *pool);

/* Return a block to the pool.
 * Thread-safe. Caller must ensure ptr was allocated from this pool. */
void mempool_free(mempool_t *pool, void *ptr);

/* Destroy the pool and free all memory.
 * All allocated blocks become invalid after this call. */
void mempool_destroy(mempool_t *pool);

/* Get pool statistics */
size_t mempool_block_size(const mempool_t *pool);
size_t mempool_total_blocks(const mempool_t *pool);
size_t mempool_free_blocks(const mempool_t *pool);

#endif
