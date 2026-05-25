/**
 * metadata.c - SQLite metadata storage implementation
 *
 * Provides file metadata, transfer logging, and user management
 * backed by a SQLite database.
 */

#include "metadata.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Schema SQL ----------------------------------------------------------- */

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS files ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    filename TEXT UNIQUE NOT NULL,"
    "    size INTEGER NOT NULL,"
    "    md5 TEXT,"
    "    upload_time TEXT DEFAULT (datetime('now')),"
    "    upload_user TEXT,"
    "    permissions INTEGER DEFAULT 644"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS transfers ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    filename TEXT NOT NULL,"
    "    user TEXT,"
    "    direction TEXT CHECK(direction IN ('upload','download')),"
    "    bytes INTEGER,"
    "    start_time TEXT DEFAULT (datetime('now')),"
    "    status TEXT DEFAULT 'ok'"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS users ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    username TEXT UNIQUE NOT NULL,"
    "    password_hash TEXT NOT NULL,"
    "    salt TEXT NOT NULL,"
    "    permissions INTEGER DEFAULT 1,"
    "    created_at TEXT DEFAULT (datetime('now')),"
    "    last_login TEXT"
    ");";

/* Helper: execute a simple SQL statement with no results */
static int exec_sql(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* Public API ----------------------------------------------------------- */

int metadata_init(metadata_t *meta, const char *db_path)
{
    if (!meta || !db_path) {
        return -1;
    }

    memset(meta, 0, sizeof(*meta));
    snprintf(meta->db_path, sizeof(meta->db_path), "%s", db_path);

    int rc = sqlite3_open(db_path, &meta->db);
    if (rc != SQLITE_OK) {
        log_error("Failed to open database %s: %s",
                  db_path, sqlite3_errmsg(meta->db));
        sqlite3_close(meta->db);
        meta->db = NULL;
        return -1;
    }

    /* Enable WAL mode for better concurrency */
    if (exec_sql(meta->db, "PRAGMA journal_mode=WAL;") != 0) {
        log_warn("Failed to enable WAL mode");
    }

    /* Create tables */
    if (exec_sql(meta->db, SCHEMA_SQL) != 0) {
        log_error("Failed to create schema");
        sqlite3_close(meta->db);
        meta->db = NULL;
        return -1;
    }

    log_info("Metadata database initialized: %s", db_path);
    return 0;
}

void metadata_close(metadata_t *meta)
{
    if (meta && meta->db) {
        sqlite3_close(meta->db);
        meta->db = NULL;
    }
}

/* File operations ------------------------------------------------------ */

int metadata_add_file(metadata_t *meta, const char *filename,
                      uint64_t size, const char *md5, const char *user)
{
    if (!meta || !meta->db || !filename) {
        return -1;
    }

    const char *sql =
        "INSERT INTO files (filename, size, md5, upload_user) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)size);
    sqlite3_bind_text(stmt, 3, md5 ? md5 : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, user ? user : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        log_error("add_file failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    log_info("File added to metadata: %s (%lu bytes)", filename,
             (unsigned long)size);
    return 0;
}

int metadata_remove_file(metadata_t *meta, const char *filename)
{
    if (!meta || !meta->db || !filename) {
        return -1;
    }

    const char *sql = "DELETE FROM files WHERE filename = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        log_error("remove_file failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    if (sqlite3_changes(meta->db) == 0) {
        log_warn("File not found in metadata: %s", filename);
        return -1;
    }

    log_info("File removed from metadata: %s", filename);
    return 0;
}

int metadata_list_files(metadata_t *meta, char *buf, size_t buf_size)
{
    if (!meta || !meta->db || !buf || buf_size == 0) {
        return -1;
    }

    const char *sql =
        "SELECT filename, size, upload_time, upload_user FROM files "
        "ORDER BY upload_time DESC;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    /* Write header */
    int written = snprintf(buf, buf_size,
                           "%-32s %-12s %-20s %-16s\n"
                           "%-32s %-12s %-20s %-16s\n",
                           "FILENAME", "SIZE", "UPLOAD_TIME", "USER",
                           "--------", "----", "------------", "----");

    if (written < 0 || (size_t)written >= buf_size) {
        sqlite3_finalize(stmt);
        return -1;
    }

    /* Write rows */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *fname = (const char *)sqlite3_column_text(stmt, 0);
        sqlite3_int64 fsize = sqlite3_column_int64(stmt, 1);
        const char *ftime = (const char *)sqlite3_column_text(stmt, 2);
        const char *fuser = (const char *)sqlite3_column_text(stmt, 3);

        int n = snprintf(buf + written, buf_size - (size_t)written,
                         "%-32s %-12llu %-20s %-16s\n",
                         fname ? fname : "",
                         (unsigned long long)fsize,
                         ftime ? ftime : "",
                         fuser ? fuser : "");
        if (n < 0 || (size_t)n >= buf_size - (size_t)written) {
            break;
        }
        written += n;
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Transfer logging ----------------------------------------------------- */

int metadata_log_transfer(metadata_t *meta, const char *filename,
                          const char *user, const char *direction,
                          uint64_t bytes)
{
    if (!meta || !meta->db || !filename || !direction) {
        return -1;
    }

    const char *sql =
        "INSERT INTO transfers (filename, user, direction, bytes) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user ? user : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, direction, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)bytes);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        log_error("log_transfer failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    log_info("Transfer logged: %s %s %s (%lu bytes)",
             user ? user : "anon", direction, filename, (unsigned long)bytes);
    return 0;
}

/* User operations ------------------------------------------------------ */

int metadata_add_user(metadata_t *meta, const char *username,
                      const char *hash, const char *salt, int perms)
{
    if (!meta || !meta->db || !username || !hash || !salt) {
        return -1;
    }

    const char *sql =
        "INSERT INTO users (username, password_hash, salt, permissions) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, perms);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        log_error("add_user failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    log_info("User added: %s (perms=%d)", username, perms);
    return 0;
}

int metadata_get_user(metadata_t *meta, const char *username,
                      char *hash, size_t hash_size,
                      char *salt, size_t salt_size, int *perms)
{
    if (!meta || !meta->db || !username || !hash || !salt || !perms) {
        return -1;
    }

    const char *sql =
        "SELECT password_hash, salt, permissions FROM users "
        "WHERE username = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(meta->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        log_warn("User not found: %s", username);
        return -1;
    }

    const char *db_hash = (const char *)sqlite3_column_text(stmt, 0);
    const char *db_salt = (const char *)sqlite3_column_text(stmt, 1);

    snprintf(hash, hash_size, "%s", db_hash ? db_hash : "");
    snprintf(salt, salt_size, "%s", db_salt ? db_salt : "");
    *perms = sqlite3_column_int(stmt, 2);

    sqlite3_finalize(stmt);
    return 0;
}

/* Statistics ----------------------------------------------------------- */

int metadata_get_stats(metadata_t *meta, int *file_count,
                       int *transfer_count, uint64_t *total_bytes)
{
    if (!meta || !meta->db) {
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    int rc;

    /* File count */
    if (file_count) {
        rc = sqlite3_prepare_v2(meta->db,
                                "SELECT COUNT(*) FROM files;",
                                -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
            return -1;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            *file_count = sqlite3_column_int(stmt, 0);
        } else {
            *file_count = 0;
        }
        sqlite3_finalize(stmt);
    }

    /* Transfer count */
    if (transfer_count) {
        rc = sqlite3_prepare_v2(meta->db,
                                "SELECT COUNT(*) FROM transfers;",
                                -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
            return -1;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            *transfer_count = sqlite3_column_int(stmt, 0);
        } else {
            *transfer_count = 0;
        }
        sqlite3_finalize(stmt);
    }

    /* Total transferred bytes */
    if (total_bytes) {
        rc = sqlite3_prepare_v2(meta->db,
                                "SELECT COALESCE(SUM(bytes), 0) FROM transfers;",
                                -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_error("prepare failed: %s", sqlite3_errmsg(meta->db));
            return -1;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            *total_bytes = (uint64_t)sqlite3_column_int64(stmt, 0);
        } else {
            *total_bytes = 0;
        }
        sqlite3_finalize(stmt);
    }

    return 0;
}
