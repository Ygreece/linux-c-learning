/**
 * http_server.c - Embedded HTTP server implementation
 *
 * Lightweight HTTP server for web admin interface.
 * Supports GET requests and JSON/HTML responses.
 */

#include "http_server.h"
#include "log.h"
#include "shm_ipc.h"
#include "server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

/* Embedded HTML page for admin interface */
static const char ADMIN_HTML[] =
#include "web_index.h"
;

/* Parse HTTP request line and headers from buffer */
static int parse_request_from_buf(const char *buf, size_t len, http_request_t *req)
{
    if (!buf || len == 0 || !req) {
        return -1;
    }

    memset(req, 0, sizeof(http_request_t));

    /* Parse request line: METHOD PATH HTTP/1.x */
    const char *line_end = memchr(buf, '\n', len);
    if (!line_end) {
        return -1;
    }

    if (sscanf(buf, "%7s %255s", req->method, req->path) != 2) {
        return -1;
    }

    /* Validate HTTP method */
    if (strcmp(req->method, "GET") != 0 &&
        strcmp(req->method, "POST") != 0 &&
        strcmp(req->method, "HEAD") != 0 &&
        strcmp(req->method, "PUT") != 0 &&
        strcmp(req->method, "DELETE") != 0) {
        return -1;
    }

    /* Parse headers */
    const char *pos = line_end + 1;
    const char *end = buf + len;

    while (pos < end) {
        const char *next = memchr(pos, '\n', end - pos);
        if (!next) {
            break;
        }

        /* Check for empty line (end of headers) */
        if (next - pos <= 1) {
            break;
        }

        /* Parse Host header */
        if (strncasecmp(pos, "Host:", 5) == 0) {
            const char *val = pos + 5;
            while (*val == ' ') val++;
            size_t vlen = next - val;
            if (vlen > 0 && val[vlen - 1] == '\r') vlen--;
            if (vlen >= sizeof(req->host)) vlen = sizeof(req->host) - 1;
            memcpy(req->host, val, vlen);
            req->host[vlen] = '\0';
        }

        /* Parse Content-Length header */
        if (strncasecmp(pos, "Content-Length:", 15) == 0) {
            req->content_length = atoi(pos + 15);
        }

        pos = next + 1;
    }

    /* Find body (after \r\n\r\n) */
    const char *body_start = strstr(buf, "\r\n\r\n");
    if (body_start) {
        const char *body_data = body_start + 4;
        /* Only set body if there's actual content after the headers */
        if (body_data < buf + len && *body_data != '\0') {
            req->body = (char *)body_data;
        }
    }

    return 0;
}

int http_parse_request(int fd, http_request_t *req, char *buf, size_t buf_size)
{
    if (fd < 0 || !req || !buf || buf_size == 0) {
        return -1;
    }

    memset(req, 0, sizeof(http_request_t));

    ssize_t n = recv(fd, buf, buf_size - 1, 0);
    if (n <= 0) {
        return -1;
    }
    buf[n] = '\0';

    return parse_request_from_buf(buf, (size_t)n, req);
}

int http_send_response(int fd, const http_response_t *resp)
{
    if (fd < 0 || !resp) {
        return -1;
    }

    char header[512];
    const char *status_text;
    switch (resp->status_code) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default:  status_text = "OK"; break;
    }

    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        resp->status_code, status_text,
        resp->content_type,
        resp->body_len);

    if (hdr_len < 0 || (size_t)hdr_len >= sizeof(header)) {
        return -1;
    }

    /* Send header */
    size_t total = 0;
    while (total < (size_t)hdr_len) {
        ssize_t n = send(fd, header + total, hdr_len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }

    /* Send body */
    if (resp->body && resp->body_len > 0) {
        total = 0;
        while (total < (size_t)resp->body_len) {
            ssize_t n = send(fd, resp->body + total, resp->body_len - total, 0);
            if (n <= 0) return -1;
            total += n;
        }
    }

    return 0;
}

int http_send_json(int fd, int status, const char *json)
{
    http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status_code = status;
    strncpy(resp.content_type, "application/json", sizeof(resp.content_type) - 1);
    resp.body = (char *)json;
    resp.body_len = json ? (int)strlen(json) : 0;
    return http_send_response(fd, &resp);
}

int http_send_html(int fd, int status, const char *html)
{
    http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status_code = status;
    strncpy(resp.content_type, "text/html", sizeof(resp.content_type) - 1);
    resp.body = (char *)html;
    resp.body_len = html ? (int)strlen(html) : 0;
    return http_send_response(fd, &resp);
}

/* Build JSON stats response */
static int build_stats_json(char *buf, size_t buf_size, void *server_ctx)
{
    int client_count = 0;
    int total_requests = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;

    /* Try shared memory first */
    shm_stats_t *shm = shm_open_existing(SHM_NAME);
    if (shm) {
        shm_get_stats(shm, &client_count, &total_requests,
                      &bytes_sent, &bytes_recv);
        shm_close(shm);
    } else if (server_ctx) {
        /* Fall back to server_context_t */
        server_context_t *ctx = (server_context_t *)server_ctx;
        client_count = ctx->client_count;
        /* total_requests and bytes not tracked in server_context_t */
    }

    return snprintf(buf, buf_size,
        "{\"client_count\":%d,"
        "\"total_requests\":%d,"
        "\"total_bytes_sent\":%llu,"
        "\"total_bytes_recv\":%llu}",
        client_count, total_requests,
        (unsigned long long)bytes_sent,
        (unsigned long long)bytes_recv);
}

/* Build JSON files response by scanning shared directory */
static int build_files_json(char *buf, size_t buf_size, const char *shared_dir)
{
    int offset = 0;
    int n;

    n = snprintf(buf + offset, buf_size - offset, "{\"files\":[");
    if (n < 0 || (size_t)n >= buf_size - offset) return -1;
    offset += n;

    DIR *dir = opendir(shared_dir);
    if (!dir) {
        n = snprintf(buf + offset, buf_size - offset, "]}");
        return offset + n;
    }

    int first = 1;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", shared_dir, entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (!first) {
                n = snprintf(buf + offset, buf_size - offset, ",");
                if (n < 0 || (size_t)n >= buf_size - offset) break;
                offset += n;
            }
            first = 0;

            n = snprintf(buf + offset, buf_size - offset,
                "{\"name\":\"%s\",\"size\":%lld}",
                entry->d_name, (long long)st.st_size);
            if (n < 0 || (size_t)n >= buf_size - offset) break;
            offset += n;
        }
    }
    closedir(dir);

    n = snprintf(buf + offset, buf_size - offset, "]}");
    if (n < 0 || (size_t)n >= buf_size - offset) return -1;
    offset += n;

    return offset;
}

/* Build JSON config response */
static int build_config_json(char *buf, size_t buf_size, void *server_ctx)
{
    if (!server_ctx) {
        return snprintf(buf, buf_size, "{}");
    }

    server_context_t *ctx = (server_context_t *)server_ctx;
    return snprintf(buf, buf_size,
        "{\"port\":%d,"
        "\"threads\":%d,"
        "\"max_clients\":%d,"
        "\"buffer_size\":%d,"
        "\"shared_dir\":\"%s\","
        "\"ssl_enabled\":%d}",
        ctx->config.port,
        ctx->config.thread_num,
        ctx->config.max_clients,
        ctx->config.buffer_size,
        ctx->config.shared_dir,
        ctx->config.ssl_enabled);
}

/* Handle a single HTTP connection */
static void handle_http_connection(int fd, void *server_ctx)
{
    char buf[4096];
    http_request_t req;

    if (http_parse_request(fd, &req, buf, sizeof(buf)) != 0) {
        http_send_json(fd, 400, "{\"error\":\"Bad request\"}");
        close(fd);
        return;
    }

    log_debug("HTTP %s %s", req.method, req.path);

    /* Route: GET / */
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/") == 0) {
        http_send_html(fd, 200, ADMIN_HTML);
        close(fd);
        return;
    }

    /* Route: GET /api/stats */
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/stats") == 0) {
        char json[256];
        build_stats_json(json, sizeof(json), server_ctx);
        http_send_json(fd, 200, json);
        close(fd);
        return;
    }

    /* Route: GET /api/files */
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/files") == 0) {
        char json[8192];
        const char *shared_dir = "./shared";
        if (server_ctx) {
            server_context_t *ctx = (server_context_t *)server_ctx;
            shared_dir = ctx->config.shared_dir;
        }
        build_files_json(json, sizeof(json), shared_dir);
        http_send_json(fd, 200, json);
        close(fd);
        return;
    }

    /* Route: GET /api/config */
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/config") == 0) {
        char json[512];
        build_config_json(json, sizeof(json), server_ctx);
        http_send_json(fd, 200, json);
        close(fd);
        return;
    }

    /* 404 Not Found */
    http_send_json(fd, 404, "{\"error\":\"Not found\"}");
    close(fd);
}

/* Server thread: accept and handle connections */
static void *http_server_thread(void *arg)
{
    http_server_t *srv = (http_server_t *)arg;
    if (!srv) {
        return NULL;
    }

    log_info("HTTP server thread started on port %d", srv->port);

    while (srv->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(srv->listen_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (!srv->running) {
                break;
            }
            log_error("HTTP accept failed: %s", strerror(errno));
            continue;
        }

        /* Handle request synchronously (single-threaded admin interface) */
        handle_http_connection(client_fd, srv->server_ctx);
    }

    log_info("HTTP server thread exiting");
    return NULL;
}

http_server_t *http_server_start(int port, void *server_ctx)
{
    http_server_t *srv = calloc(1, sizeof(http_server_t));
    if (!srv) {
        log_error("Failed to allocate http_server_t");
        return NULL;
    }

    srv->port = port;
    srv->server_ctx = server_ctx;
    srv->running = 1;
    srv->listen_fd = -1;

    /* Create listening socket */
    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) {
        log_error("HTTP server: failed to create socket: %s", strerror(errno));
        free(srv);
        return NULL;
    }

    /* Allow address reuse */
    int opt = 1;
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("HTTP server: failed to bind port %d: %s", port, strerror(errno));
        close(srv->listen_fd);
        free(srv);
        return NULL;
    }

    /* Start listening */
    if (listen(srv->listen_fd, 16) < 0) {
        log_error("HTTP server: failed to listen: %s", strerror(errno));
        close(srv->listen_fd);
        free(srv);
        return NULL;
    }

    /* Spawn server thread */
    if (pthread_create(&srv->thread, NULL, http_server_thread, srv) != 0) {
        log_error("HTTP server: failed to create thread: %s", strerror(errno));
        close(srv->listen_fd);
        free(srv);
        return NULL;
    }

    log_info("HTTP server started on port %d", port);
    return srv;
}

void http_server_stop(http_server_t *srv)
{
    if (!srv) {
        return;
    }

    srv->running = 0;

    /* Close listen socket to unblock accept() */
    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }

    /* Wait for thread to finish */
    pthread_join(srv->thread, NULL);

    log_info("HTTP server stopped");
    free(srv);
}
