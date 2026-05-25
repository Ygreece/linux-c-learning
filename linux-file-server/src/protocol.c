/**
 * protocol.c - Protocol module implementation
 *
 * Provides shared protocol utilities used by both server and client:
 * - Reliable send_all / recv_all that handle partial reads/writes
 * - Protocol header send/receive helpers
 * - Checksum calculation
 */

#include "protocol.h"
#include "log.h"

/* Reliable send: loops until all bytes are sent or an error occurs */
int send_all(int fd, const void *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, (const char *)buf + total, len - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

/* Reliable recv: loops until all bytes are received or an error occurs */
int recv_all(int fd, void *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(fd, (char *)buf + total, len - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

/* Send a protocol header over the wire */
int protocol_send_header(int fd, const protocol_header_t *header)
{
    if (!header) {
        return -1;
    }
    return send_all(fd, header, sizeof(protocol_header_t));
}

/* Receive a protocol header from the wire */
int protocol_recv_header(int fd, protocol_header_t *header)
{
    if (!header) {
        return -1;
    }
    return recv_all(fd, header, sizeof(protocol_header_t));
}

/* Send a response with optional message */
int protocol_send_response(int fd, status_code_t status, const char *msg)
{
    response_header_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status = (uint32_t)status;
    resp.data_len = 0;
    if (msg) {
        strncpy(resp.message, msg, sizeof(resp.message) - 1);
    }
    return send_all(fd, &resp, sizeof(resp));
}

/* Receive a response */
int protocol_recv_response(int fd, response_header_t *resp)
{
    if (!resp) {
        return -1;
    }
    return recv_all(fd, resp, sizeof(response_header_t));
}

/* Simple additive checksum */
uint32_t protocol_checksum(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += p[i];
    }
    return sum;
}
