#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int fd;
    char device[256];
    int baud_rate;
    int data_bits;     /* 5, 6, 7, 8 */
    int stop_bits;     /* 1, 2 */
    char parity;       /* 'N', 'E', 'O' */
    struct termios old_termios;
    int is_open;
    int hex_mode;      /* 1 = display as hex */
    int timestamp;     /* 1 = show timestamps */
    int log_fd;        /* File descriptor for logging */
} serial_port_t;

/* Open serial port */
int serial_open(serial_port_t *port, const char *device, int baud);

/* Close serial port */
void serial_close(serial_port_t *port);

/* Configure port parameters */
int serial_configure(serial_port_t *port, int baud, int data_bits,
                     int stop_bits, char parity);

/* Send data */
int serial_send(serial_port_t *port, const void *data, size_t len);

/* Send string */
int serial_send_str(serial_port_t *port, const char *str);

/* Send hex bytes (e.g., "FF 01 A3") */
int serial_send_hex(serial_port_t *port, const char *hex_str);

/* Receive data (non-blocking with timeout) */
int serial_recv(serial_port_t *port, void *buf, size_t max_len, int timeout_ms);

/* Flush input/output buffers */
int serial_flush(serial_port_t *port);

/* Get baud rate constant from integer */
speed_t serial_baud_const(int baud);

/* List available serial ports */
int serial_list_ports(char ports[][256], int max_ports);

#endif
