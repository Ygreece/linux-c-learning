/**
 * test_protocol.c - Protocol module tests
 *
 * Tests:
 * - Checksum consistency (same data -> same checksum)
 * - Checksum sensitivity (different data -> different checksum)
 * - send_all / recv_all over a pipe (reliable transfer)
 * - protocol_send_header / protocol_recv_header round-trip
 * - protocol_send_response / protocol_recv_response round-trip
 */

#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Helper: create a connected socketpair */
static void create_pair(int fds[2])
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
}

void test_checksum_consistency(void)
{
    const char *data = "Hello, protocol!";
    size_t len = strlen(data);

    uint32_t c1 = protocol_checksum(data, len);
    uint32_t c2 = protocol_checksum(data, len);

    assert(c1 == c2);
    printf("  [PASS] checksum consistency\n");
}

void test_checksum_sensitivity(void)
{
    const char *data1 = "Hello";
    const char *data2 = "World";

    uint32_t c1 = protocol_checksum(data1, strlen(data1));
    uint32_t c2 = protocol_checksum(data2, strlen(data2));

    assert(c1 != c2);
    printf("  [PASS] checksum sensitivity\n");
}

void test_checksum_empty(void)
{
    uint32_t c = protocol_checksum("", 0);
    assert(c == 0);
    printf("  [PASS] checksum empty input\n");
}

void test_send_recv_all_basic(void)
{
    int fds[2];
    create_pair(fds);

    const char *msg = "Test data for send_all and recv_all";
    size_t len = strlen(msg);

    assert(send_all(fds[1], msg, len) == 0);

    char buf[128] = {0};
    assert(recv_all(fds[0], buf, len) == 0);

    assert(memcmp(msg, buf, len) == 0);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] send_all/recv_all basic transfer\n");
}

void test_send_recv_all_large(void)
{
    int fds[2];
    create_pair(fds);

    /* 64KB buffer to exercise partial reads/writes */
    size_t len = 65536;
    char *send_buf = malloc(len);
    char *recv_buf = malloc(len);
    assert(send_buf && recv_buf);

    for (size_t i = 0; i < len; i++) {
        send_buf[i] = (char)(i & 0xFF);
    }

    assert(send_all(fds[1], send_buf, len) == 0);
    assert(recv_all(fds[0], recv_buf, len) == 0);
    assert(memcmp(send_buf, recv_buf, len) == 0);

    free(send_buf);
    free(recv_buf);
    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] send_all/recv_all large transfer (64KB)\n");
}

void test_send_recv_all_byte_by_byte(void)
{
    int fds[2];
    create_pair(fds);

    /* Send one byte at a time */
    for (int i = 0; i < 100; i++) {
        char c = (char)i;
        assert(send_all(fds[1], &c, 1) == 0);
    }

    /* Receive one byte at a time */
    for (int i = 0; i < 100; i++) {
        char c = 0;
        assert(recv_all(fds[0], &c, 1) == 0);
        assert(c == (char)i);
    }

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] send_all/recv_all byte-by-byte\n");
}

void test_protocol_header_roundtrip(void)
{
    int fds[2];
    create_pair(fds);

    protocol_header_t hdr_out;
    memset(&hdr_out, 0, sizeof(hdr_out));
    hdr_out.magic = PROTOCOL_MAGIC;
    hdr_out.cmd = CMD_LIST;
    hdr_out.data_len = 12345;
    hdr_out.checksum = 999;
    hdr_out.offset = 9876543210ULL;
    strncpy(hdr_out.filename, "testfile.txt", sizeof(hdr_out.filename) - 1);

    assert(protocol_send_header(fds[1], &hdr_out) == 0);

    protocol_header_t hdr_in;
    memset(&hdr_in, 0, sizeof(hdr_in));
    assert(protocol_recv_header(fds[0], &hdr_in) == 0);

    assert(hdr_in.magic == PROTOCOL_MAGIC);
    assert(hdr_in.cmd == CMD_LIST);
    assert(hdr_in.data_len == 12345);
    assert(hdr_in.checksum == 999);
    assert(hdr_in.offset == 9876543210ULL);
    assert(strcmp(hdr_in.filename, "testfile.txt") == 0);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] protocol header round-trip\n");
}

void test_protocol_response_roundtrip(void)
{
    int fds[2];
    create_pair(fds);

    assert(protocol_send_response(fds[1], STATUS_OK, "All good") == 0);

    response_header_t resp;
    memset(&resp, 0, sizeof(resp));
    assert(protocol_recv_response(fds[0], &resp) == 0);

    assert(resp.status == STATUS_OK);
    assert(resp.data_len == 0);
    assert(strcmp(resp.message, "All good") == 0);

    close(fds[0]);
    close(fds[1]);
    printf("  [PASS] protocol response round-trip\n");
}

void test_recv_all_closed_peer(void)
{
    int fds[2];
    create_pair(fds);

    /* Close the writer side */
    close(fds[1]);

    char buf[16];
    int ret = recv_all(fds[0], buf, sizeof(buf));
    assert(ret == -1);

    close(fds[0]);
    printf("  [PASS] recv_all returns -1 on closed peer\n");
}

int main(void)
{
    printf("=== Protocol module tests ===\n");

    printf("Checksum tests:\n");
    test_checksum_consistency();
    test_checksum_sensitivity();
    test_checksum_empty();

    printf("send_all/recv_all tests:\n");
    test_send_recv_all_basic();
    test_send_recv_all_large();
    test_send_recv_all_byte_by_byte();

    printf("Protocol header/response tests:\n");
    test_protocol_header_roundtrip();
    test_protocol_response_roundtrip();

    printf("Error handling tests:\n");
    test_recv_all_closed_peer();

    printf("\nAll protocol tests passed!\n");
    return 0;
}
