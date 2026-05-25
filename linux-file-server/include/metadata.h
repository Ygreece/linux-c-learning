/**
 * metadata.h - SQLite metadata storage
 *
 * Stores file metadata, transfer logs, and user records in SQLite.
 */

#ifndef METADATA_H
#define METADATA_H

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    sqlite3 *db;
    char db_path[256];
} metadata_t;

/* Initialize database (creates tables if not exist) */
int metadata_init(metadata_t *meta, const char *db_path);
void metadata_close(metadata_t *meta);

/* File operations */
int metadata_add_file(metadata_t *meta, const char *filename,
                      uint64_t size, const char *md5, const char *user);
int metadata_remove_file(metadata_t *meta, const char *filename);
int metadata_list_files(metadata_t *meta, char *buf, size_t buf_size);

/* Transfer logging */
int metadata_log_transfer(metadata_t *meta, const char *filename,
                          const char *user, const char *direction,
                          uint64_t bytes);

/* User operations */
int metadata_add_user(metadata_t *meta, const char *username,
                      const char *hash, const char *salt, int perms);
int metadata_get_user(metadata_t *meta, const char *username,
                      char *hash, size_t hash_size,
                      char *salt, size_t salt_size, int *perms);

/* Statistics */
int metadata_get_stats(metadata_t *meta, int *file_count,
                       int *transfer_count, uint64_t *total_bytes);

#endif /* METADATA_H */
