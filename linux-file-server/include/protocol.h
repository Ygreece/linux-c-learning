/**
 * protocol.h - Protocol module shared between server and client
 *
 * Extracts common protocol utilities (send_all, recv_all, checksum)
 * to eliminate duplication between server.c and client.c.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

#define PROTOCOL_VERSION_V1 1
#define PROTOCOL_VERSION_V2 2

/* Protocol flag bits */
#define FLAG_COMPRESSED  0x01
#define FLAG_ENCRYPTED   0x02
#define FLAG_RESUME      0x04

/* Send/receive protocol header (v1 compatibility) */
int protocol_send_header(int fd, const protocol_header_t *header);
int protocol_recv_header(int fd, protocol_header_t *header);

/* Send/receive response */
int protocol_send_response(int fd, status_code_t status, const char *msg);
int protocol_recv_response(int fd, response_header_t *resp);

/* Reliable send/recv - these replace the duplicate functions in server.c and client.c */
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

/* Checksum calculation */
uint32_t protocol_checksum(const void *data, size_t len);

#endif /* PROTOCOL_H */
