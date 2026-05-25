#ifndef SCRIPT_H
#define SCRIPT_H

#include "serial.h"

/* Run a script file with serial commands */
int script_run(serial_port_t *port, const char *script_file);

/* Send a command and wait for response */
int script_send_recv(serial_port_t *port, const char *send,
                     char *recv_buf, size_t recv_size,
                     const char *expected, int timeout_ms);

#endif
