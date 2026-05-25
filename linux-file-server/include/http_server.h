/**
 * http_server.h - Embedded HTTP server for web admin interface
 *
 * Provides a lightweight HTTP server that serves:
 * - Static HTML admin page
 * - JSON API endpoints for server statistics and file listing
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "common.h"

/* HTTP request (parsed) */
typedef struct {
    char method[8];       /* GET/POST */
    char path[256];       /* URL path */
    char host[128];       /* Host header */
    int content_length;   /* Content-Length header */
    char *body;           /* Points into buffer, not allocated separately */
} http_request_t;

/* HTTP response (to send) */
typedef struct {
    int status_code;
    char content_type[64];
    char *body;
    int body_len;
} http_response_t;

/* HTTP server instance */
typedef struct http_server http_server_t;

struct http_server {
    int listen_fd;
    int port;
    volatile int running;
    pthread_t thread;
    void *server_ctx;  /* Pointer to server_context_t for stats */
};

/* Create and start HTTP server (non-blocking, runs on separate thread) */
http_server_t *http_server_start(int port, void *server_ctx);

/* Stop HTTP server and free resources */
void http_server_stop(http_server_t *srv);

/* Parse HTTP request from fd into req struct */
int http_parse_request(int fd, http_request_t *req, char *buf, size_t buf_size);

/* Send HTTP response with custom status and content type */
int http_send_response(int fd, const http_response_t *resp);

/* Send JSON response (Content-Type: application/json) */
int http_send_json(int fd, int status, const char *json);

/* Send HTML response (Content-Type: text/html) */
int http_send_html(int fd, int status, const char *html);

#endif /* HTTP_SERVER_H */
