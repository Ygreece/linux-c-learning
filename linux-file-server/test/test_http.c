/**
 * test_http.c - HTTP server module tests
 *
 * Tests:
 * - http_parse_request: parse GET request, verify fields
 * - http_send_json: send JSON response, verify format
 * - http_send_html: send HTML response, verify format
 * - http_send_response: custom response, verify format
 */

#include "http_server.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

/* Helper: create a connected socketpair */
static void create_pair(int fds[2])
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
}

/* Helper: send raw HTTP request to fd */
static void send_raw_request(int fd, const char *request)
{
    size_t len = strlen(request);
    ssize_t n = send(fd, request, len, 0);
    assert(n == (ssize_t)len);
}

/* Helper: read all data from fd (writer side must be closed first) */
static size_t read_all(int fd, char *buf, size_t buf_size)
{
    size_t total = 0;
    ssize_t n;
    while (total < buf_size - 1) {
        n = recv(fd, buf + total, buf_size - total - 1, 0);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    return total;
}

void test_parse_get_request(void)
{
    int fds[2];
    create_pair(fds);

    const char *request =
        "GET /api/stats HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Connection: close\r\n"
        "\r\n";

    send_raw_request(fds[1], request);

    char buf[4096];
    http_request_t req;
    int ret = http_parse_request(fds[0], &req, buf, sizeof(buf));
    assert(ret == 0);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/api/stats") == 0);
    assert(strcmp(req.host, "localhost:8080") == 0);
    assert(req.content_length == 0);
    assert(req.body == NULL);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] parse GET request\n");
}

void test_parse_post_request(void)
{
    int fds[2];
    create_pair(fds);

    const char *request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";

    send_raw_request(fds[1], request);

    char buf[4096];
    http_request_t req;
    int ret = http_parse_request(fds[0], &req, buf, sizeof(buf));
    assert(ret == 0);
    assert(strcmp(req.method, "POST") == 0);
    assert(strcmp(req.path, "/upload") == 0);
    assert(req.content_length == 11);
    assert(req.body != NULL);
    assert(memcmp(req.body, "hello world", 11) == 0);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] parse POST request with body\n");
}

void test_parse_invalid_request(void)
{
    int fds[2];
    create_pair(fds);

    /* Send invalid request (no valid HTTP method) */
    send_raw_request(fds[1], "garbage data\r\n\r\n");

    char buf[4096];
    http_request_t req;
    int ret = http_parse_request(fds[0], &req, buf, sizeof(buf));
    assert(ret == -1);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] parse invalid request returns -1\n");
}

void test_parse_empty_request(void)
{
    int fds[2];
    create_pair(fds);

    /* Close writer side immediately */
    close(fds[1]);

    char buf[4096];
    http_request_t req;
    int ret = http_parse_request(fds[0], &req, buf, sizeof(buf));
    assert(ret == -1);

    close(fds[0]);
    printf("  [PASS] parse empty/closed request returns -1\n");
}

void test_send_json(void)
{
    int fds[2];
    create_pair(fds);

    const char *json = "{\"status\":\"ok\",\"count\":42}";
    int ret = http_send_json(fds[1], 200, json);
    assert(ret == 0);

    /* Close writer side so reader gets EOF after all data */
    close(fds[1]);

    char resp[1024];
    size_t len = read_all(fds[0], resp, sizeof(resp));
    assert(len > 0);

    /* Verify status line */
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);
    /* Verify content type */
    assert(strstr(resp, "Content-Type: application/json") != NULL);
    /* Verify body */
    assert(strstr(resp, "{\"status\":\"ok\",\"count\":42}") != NULL);
    /* Verify Content-Length */
    assert(strstr(resp, "Content-Length: 26") != NULL);

    close(fds[0]);
    printf("  [PASS] send JSON response\n");
}

void test_send_html(void)
{
    int fds[2];
    create_pair(fds);

    const char *html = "<html><body>Hello</body></html>";
    int ret = http_send_html(fds[1], 200, html);
    assert(ret == 0);

    close(fds[1]);

    char resp[1024];
    size_t len = read_all(fds[0], resp, sizeof(resp));
    assert(len > 0);

    /* Verify status line */
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);
    /* Verify content type */
    assert(strstr(resp, "Content-Type: text/html") != NULL);
    /* Verify body */
    assert(strstr(resp, "<html><body>Hello</body></html>") != NULL);

    close(fds[0]);
    printf("  [PASS] send HTML response\n");
}

void test_send_json_404(void)
{
    int fds[2];
    create_pair(fds);

    const char *json = "{\"error\":\"Not found\"}";
    int ret = http_send_json(fds[1], 404, json);
    assert(ret == 0);

    close(fds[1]);

    char resp[1024];
    size_t len = read_all(fds[0], resp, sizeof(resp));
    assert(len > 0);

    /* Verify 404 status */
    assert(strstr(resp, "HTTP/1.1 404 Not Found") != NULL);
    assert(strstr(resp, "{\"error\":\"Not found\"}") != NULL);

    close(fds[0]);
    printf("  [PASS] send JSON 404 response\n");
}

void test_send_response_null_body(void)
{
    int fds[2];
    create_pair(fds);

    int ret = http_send_json(fds[1], 200, NULL);
    assert(ret == 0);

    close(fds[1]);

    char resp[512];
    size_t len = read_all(fds[0], resp, sizeof(resp));
    assert(len > 0);

    /* Verify Content-Length is 0 */
    assert(strstr(resp, "Content-Length: 0") != NULL);

    close(fds[0]);
    printf("  [PASS] send response with NULL body\n");
}

void test_send_response_500(void)
{
    int fds[2];
    create_pair(fds);

    const char *json = "{\"error\":\"Internal server error\"}";
    int ret = http_send_json(fds[1], 500, json);
    assert(ret == 0);

    close(fds[1]);

    char resp[1024];
    size_t len = read_all(fds[0], resp, sizeof(resp));
    assert(len > 0);

    /* Verify 500 status */
    assert(strstr(resp, "HTTP/1.1 500 Internal Server Error") != NULL);

    close(fds[0]);
    printf("  [PASS] send JSON 500 response\n");
}

int main(void)
{
    printf("=== HTTP server module tests ===\n");

    printf("Request parsing tests:\n");
    test_parse_get_request();
    test_parse_post_request();
    test_parse_invalid_request();
    test_parse_empty_request();

    printf("Response sending tests:\n");
    test_send_json();
    test_send_html();
    test_send_json_404();
    test_send_response_null_body();
    test_send_response_500();

    printf("\nAll HTTP server tests passed!\n");
    return 0;
}
