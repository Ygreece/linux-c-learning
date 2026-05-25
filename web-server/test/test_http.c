/**
 * test_http.c - HTTP服务器单元测试
 */

#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASSED printf("  ✓ %s\n", __func__)

/* 测试HTTP方法解析 */
void test_method_parse(void) {
    assert(http_method_parse("GET") == HTTP_METHOD_GET);
    assert(http_method_parse("POST") == HTTP_METHOD_POST);
    assert(http_method_parse("PUT") == HTTP_METHOD_PUT);
    assert(http_method_parse("DELETE") == HTTP_METHOD_DELETE);
    assert(http_method_parse("HEAD") == HTTP_METHOD_HEAD);
    assert(http_method_parse("OPTIONS") == HTTP_METHOD_OPTIONS);
    assert(http_method_parse("UNKNOWN") == HTTP_METHOD_UNKNOWN);
    assert(http_method_parse("INVALID") == HTTP_METHOD_UNKNOWN);

    TEST_PASSED;
}

/* 测试HTTP方法名称 */
void test_method_name(void) {
    assert(strcmp(http_method_name(HTTP_METHOD_GET), "GET") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_POST), "POST") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_PUT), "PUT") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_DELETE), "DELETE") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_HEAD), "HEAD") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_OPTIONS), "OPTIONS") == 0);
    assert(strcmp(http_method_name(HTTP_METHOD_UNKNOWN), "UNKNOWN") == 0);

    TEST_PASSED;
}

/* 测试HTTP状态码文本 */
void test_status_text(void) {
    assert(strcmp(http_status_text(HTTP_STATUS_OK), "OK") == 0);
    assert(strcmp(http_status_text(HTTP_STATUS_NOT_FOUND), "Not Found") == 0);
    assert(strcmp(http_status_text(HTTP_STATUS_INTERNAL_ERROR), "Internal Server Error") == 0);
    assert(strcmp(http_status_text(HTTP_STATUS_BAD_REQUEST), "Bad Request") == 0);

    TEST_PASSED;
}

/* 测试MIME类型 */
void test_mime_type(void) {
    assert(strcmp(http_mime_type(".html"), "text/html") == 0);
    assert(strcmp(http_mime_type(".css"), "text/css") == 0);
    assert(strcmp(http_mime_type(".js"), "application/javascript") == 0);
    assert(strcmp(http_mime_type(".json"), "application/json") == 0);
    assert(strcmp(http_mime_type(".jpg"), "image/jpeg") == 0);
    assert(strcmp(http_mime_type(".png"), "image/png") == 0);
    assert(strcmp(http_mime_type(".pdf"), "application/pdf") == 0);
    assert(strcmp(http_mime_type(".unknown"), "application/octet-stream") == 0);

    TEST_PASSED;
}

/* 测试URL解码 */
void test_url_decode(void) {
    char decoded[256];

    http_url_decode("hello%20world", decoded, sizeof(decoded));
    assert(strcmp(decoded, "hello world") == 0);

    http_url_decode("a%2Bb%3Dc", decoded, sizeof(decoded));
    assert(strcmp(decoded, "a+b=c") == 0);

    http_url_decode("normal+text", decoded, sizeof(decoded));
    assert(strcmp(decoded, "normal text") == 0);

    http_url_decode("100%25", decoded, sizeof(decoded));
    assert(strcmp(decoded, "100%") == 0);

    TEST_PASSED;
}

/* 测试URL编码 */
void test_url_encode(void) {
    char encoded[256];

    http_url_encode("hello world", encoded, sizeof(encoded));
    assert(strcmp(encoded, "hello%20world") == 0);

    http_url_encode("a+b=c", encoded, sizeof(encoded));
    assert(strcmp(encoded, "a%2Bb%3Dc") == 0);

    http_url_encode("normal", encoded, sizeof(encoded));
    assert(strcmp(encoded, "normal") == 0);

    TEST_PASSED;
}

/* 测试HTTP请求解析 */
void test_request_parse(void) {
    const char *raw_request =
        "GET /index.html?name=test HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    http_request_t request;
    int ret = http_request_parse(raw_request, strlen(raw_request), &request);
    assert(ret == 0);

    assert(request.method == HTTP_METHOD_GET);
    assert(strcmp(request.path, "/index.html") == 0);
    assert(strcmp(request.query_string, "name=test") == 0);
    assert(request.version == HTTP_VERSION_1_1);
    assert(strcmp(request.headers.host, "localhost:8080") == 0);
    assert(strcmp(request.headers.user_agent, "TestClient/1.0") == 0);
    assert(request.headers.keep_alive == 1);

    http_request_free(&request);
    TEST_PASSED;
}

/* 测试HTTP请求解析（带body）*/
void test_request_parse_with_body(void) {
    const char *raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "{\"key\":\"val\"}";

    http_request_t request;
    int ret = http_request_parse(raw_request, strlen(raw_request), &request);
    assert(ret == 0);

    assert(request.method == HTTP_METHOD_POST);
    assert(strcmp(request.path, "/api/data") == 0);
    assert(request.body_length == 13);
    assert(strcmp(request.body, "{\"key\":\"val\"}") == 0);

    http_request_free(&request);
    TEST_PASSED;
}

/* 测试HTTP响应构建 */
void test_response_build(void) {
    http_response_t response;
    memset(&response, 0, sizeof(response));

    response.status = HTTP_STATUS_OK;
    response.content_type = "text/plain";
    response.body = "Hello, World!";
    response.body_length = 13;

    char buffer[1024];
    size_t length = http_response_build(&response, buffer, sizeof(buffer));

    assert(length > 0);
    assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(buffer, "Content-Type: text/plain") != NULL);
    assert(strstr(buffer, "Content-Length: 13") != NULL);
    assert(strstr(buffer, "Hello, World!") != NULL);

    TEST_PASSED;
}

/* 测试文件大小 */
void test_file_size(void) {
    /* 创建临时文件 */
    FILE *fp = fopen("/tmp/test_file_size.txt", "w");
    if (fp) {
        fprintf(fp, "Hello, World!");
        fclose(fp);

        long size = http_file_size("/tmp/test_file_size.txt");
        assert(size == 13);

        remove("/tmp/test_file_size.txt");
    }

    TEST_PASSED;
}

int main(void) {
    printf("=== HTTP Server Tests ===\n");

    test_method_parse();
    test_method_name();
    test_status_text();
    test_mime_type();
    test_url_decode();
    test_url_encode();
    test_request_parse();
    test_request_parse_with_body();
    test_response_build();
    test_file_size();

    printf("\n=== All tests passed! ===\n");
    return 0;
}
