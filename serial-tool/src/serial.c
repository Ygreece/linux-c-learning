#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <dirent.h>
#include <ctype.h>

/* Suppress warn_unused_result for fire-and-forget log writes */
static inline void safe_write(int fd, const void *buf, size_t count) {
    ssize_t ret = write(fd, buf, count);
    (void)ret;
}

speed_t serial_baud_const(int baud) {
    switch (baud) {
        case 50:      return B50;
        case 75:      return B75;
        case 110:     return B110;
        case 134:     return B134;
        case 150:     return B150;
        case 200:     return B200;
        case 300:     return B300;
        case 600:     return B600;
        case 1200:    return B1200;
        case 1800:    return B1800;
        case 2400:    return B2400;
        case 4800:    return B4800;
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
#ifdef B460800
        case 460800:  return B460800;
#endif
#ifdef B500000
        case 500000:  return B500000;
#endif
#ifdef B576000
        case 576000:  return B576000;
#endif
#ifdef B921600
        case 921600:  return B921600;
#endif
#ifdef B1000000
        case 1000000: return B1000000;
#endif
#ifdef B1500000
        case 1500000: return B1500000;
#endif
#ifdef B2000000
        case 2000000: return B2000000;
#endif
        default:
            fprintf(stderr, "Unsupported baud rate: %d\n", baud);
            return B115200;
    }
}

int serial_open(serial_port_t *port, const char *device, int baud) {
    if (!port || !device) return -1;

    memset(port, 0, sizeof(*port));
    port->fd = -1;
    port->log_fd = -1;

    /* Open the serial port */
    port->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
        return -1;
    }

    /* Save original termios settings */
    if (tcgetattr(port->fd, &port->old_termios) < 0) {
        fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
        close(port->fd);
        port->fd = -1;
        return -1;
    }

    strncpy(port->device, device, sizeof(port->device) - 1);
    port->device[sizeof(port->device) - 1] = '\0';
    port->baud_rate = baud;
    port->is_open = 1;
    port->hex_mode = 0;
    port->timestamp = 0;

    return 0;
}

int serial_configure(serial_port_t *port, int baud, int data_bits,
                     int stop_bits, char parity) {
    if (!port || !port->is_open) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(port->fd, &tty) < 0) {
        fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
        return -1;
    }

    /* Set baud rate */
    speed_t speed = serial_baud_const(baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* Set input flags - raw mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                      INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);

    /* Set output flags - raw mode */
    tty.c_oflag &= ~OPOST;

    /* Set control flags */
    tty.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cflag |= CREAD | CLOCAL;

    /* Data bits */
    switch (data_bits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: tty.c_cflag |= CS8; break;
        default:
            fprintf(stderr, "Invalid data bits: %d, using 8\n", data_bits);
            tty.c_cflag |= CS8;
            data_bits = 8;
    }

    /* Stop bits */
    if (stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    }

    /* Parity */
    switch (toupper((unsigned char)parity)) {
        case 'E':
            tty.c_cflag |= PARENB;
            break;
        case 'O':
            tty.c_cflag |= PARENB | PARODD;
            break;
        case 'N':
        default:
            /* No parity - already cleared */
            break;
    }

    /* Set local flags - raw mode (disable canonical, echo, signals) */
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    /* Blocking read with timeout */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;  /* We use select() for timeout */

    if (tcsetattr(port->fd, TCSANOW, &tty) < 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
        return -1;
    }

    /* Flush any pending I/O */
    tcflush(port->fd, TCIOFLUSH);

    port->baud_rate = baud;
    port->data_bits = data_bits;
    port->stop_bits = stop_bits;
    port->parity = toupper((unsigned char)parity);

    return 0;
}

int serial_send(serial_port_t *port, const void *data, size_t len) {
    if (!port || !port->is_open || !data) return -1;

    const unsigned char *ptr = (const unsigned char *)data;
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = write(port->fd, ptr + total_sent, len - total_sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Wait a bit and retry */
                usleep(1000);
                continue;
            }
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            return -1;
        }
        total_sent += (size_t)n;
    }

    /* Wait for output to be transmitted */
    tcdrain(port->fd);

    /* Log sent data */
    if (port->log_fd >= 0) {
        const char prefix[] = ">> ";
        safe_write(port->log_fd, prefix, sizeof(prefix) - 1);
        safe_write(port->log_fd, data, len);
        safe_write(port->log_fd, "\n", 1);
    }

    return (int)total_sent;
}

int serial_send_str(serial_port_t *port, const char *str) {
    if (!str) return -1;
    return serial_send(port, str, strlen(str));
}

int serial_send_hex(serial_port_t *port, const char *hex_str) {
    if (!port || !hex_str) return -1;

    /* Parse hex string like "FF 01 A3" or "FF01A3" */
    unsigned char buf[4096];
    size_t count = 0;
    const char *p = hex_str;

    while (*p && count < sizeof(buf)) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        /* Parse hex byte */
        char hex[3] = {0};
        hex[0] = *p++;
        if (*p && *p != ' ' && *p != '\t') {
            hex[1] = *p++;
        }

        char *end;
        long val = strtol(hex, &end, 16);
        if (end == hex) {
            fprintf(stderr, "Invalid hex character at: %s\n", p - 1);
            return -1;
        }
        buf[count++] = (unsigned char)val;
    }

    if (count == 0) return 0;
    return serial_send(port, buf, count);
}

int serial_recv(serial_port_t *port, void *buf, size_t max_len, int timeout_ms) {
    if (!port || !port->is_open || !buf) return -1;

    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(port->fd, &readfds);

    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(port->fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        if (errno == EINTR) return 0;
        fprintf(stderr, "select error: %s\n", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        return 0; /* Timeout, no data */
    }

    ssize_t n = read(port->fd, buf, max_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) return 0;
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        return -1;
    }

    return (int)n;
}

int serial_flush(serial_port_t *port) {
    if (!port || !port->is_open) return -1;
    return tcflush(port->fd, TCIOFLUSH);
}

void serial_close(serial_port_t *port) {
    if (!port) return;

    if (port->is_open && port->fd >= 0) {
        /* Restore original termios settings */
        tcsetattr(port->fd, TCSANOW, &port->old_termios);
        close(port->fd);
    }

    if (port->log_fd >= 0) {
        close(port->log_fd);
    }

    port->fd = -1;
    port->log_fd = -1;
    port->is_open = 0;
}

int serial_list_ports(char ports[][256], int max_ports) {
    int count = 0;

    /* Patterns to scan */
    static const char *patterns[] = {
        "/dev/ttyUSB",
        "/dev/ttyACM",
        "/dev/ttyS",
        NULL
    };

    for (int p = 0; patterns[p] && count < max_ports; p++) {
        const char *dir_path = "/dev";
        DIR *dir = opendir(dir_path);
        if (!dir) continue;

        size_t prefix_len = strlen(patterns[p]);
        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL && count < max_ports) {
            /* Check if name starts with our pattern */
            char full_path[256];
            const char *name = entry->d_name;

            /* Match prefix: e.g. "ttyUSB" matches pattern "/dev/ttyUSB" */
            if (strncmp(name, patterns[p] + 5, prefix_len - 5) != 0) {
                /* Also match the full pattern path format */
                if (strncmp(patterns[p], "/dev/", 5) == 0) {
                    if (strncmp(name, patterns[p] + 5, prefix_len - 5) != 0)
                        continue;
                }
            }

            snprintf(full_path, sizeof(full_path), "/dev/%s", name);

            /* Try to open the device to verify it exists */
            int fd = open(full_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (fd >= 0) {
                close(fd);
                snprintf(ports[count], 256, "%s", full_path);
                count++;
            }
        }
        closedir(dir);
    }

    return count;
}
