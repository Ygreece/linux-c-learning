#include "terminal.h"
#include "hexdump.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

/* Suppress warn_unused_result for fire-and-forget log writes */
static inline void safe_write(int fd, const void *buf, size_t count) {
    ssize_t ret = write(fd, buf, count);
    (void)ret;
}

void terminal_help(void) {
    printf("\n=== Serial Terminal Help ===\n");
    printf("  Ctrl+A ?    - Show this help\n");
    printf("  Ctrl+A q    - Quit\n");
    printf("  Ctrl+A h    - Toggle hex display mode\n");
    printf("  Ctrl+A t    - Toggle timestamps\n");
    printf("  Ctrl+A e    - Toggle local echo\n");
    printf("  Ctrl+A l    - Toggle line mode\n");
    printf("  Ctrl+A s    - Send file\n");
    printf("  Ctrl+A c    - Send Ctrl+C (break)\n");
    printf("  Ctrl+A d    - Send Ctrl+D\n");
    printf("============================\n\n");
}

/* Put terminal into raw mode */
static int set_raw_mode(struct termios *orig) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) return -1;

    if (tcgetattr(STDIN_FILENO, orig) < 0) {
        perror("tcgetattr stdin");
        return -1;
    }

    raw = *orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr stdin");
        return -1;
    }

    return 0;
}

/* Send a file over the serial port */
static int send_file(serial_port_t *port, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("\nCannot open file: %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("\nSending file %s (%ld bytes)...\n", filename, file_size);

    char buf[1024];
    size_t total_sent = 0;
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        int ret = serial_send(port, buf, n);
        if (ret < 0) {
            printf("\nSend error at byte %zu\n", total_sent);
            fclose(fp);
            return -1;
        }
        total_sent += (size_t)ret;

        /* Progress indicator */
        if (file_size > 0) {
            printf("\rProgress: %zu/%ld bytes (%d%%)",
                   total_sent, file_size,
                   (int)(total_sent * 100 / (size_t)file_size));
            fflush(stdout);
        }
    }

    fclose(fp);
    printf("\nFile sent: %zu bytes\n", total_sent);
    return (int)total_sent;
}

/* Print received data based on mode settings */
static void display_data(serial_port_t *port, const void *data, int len) {
    if (port->hex_mode) {
        hexdump_print(data, len, 0);
    } else if (port->timestamp) {
        hexdump_timestamp(data, len);
    } else {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
    }

    /* Write to log file if open */
    if (port->log_fd >= 0) {
        const char prefix[] = "<< ";
        safe_write(port->log_fd, prefix, sizeof(prefix) - 1);
        safe_write(port->log_fd, data, len);
        safe_write(port->log_fd, "\n", 1);
    }
}

int terminal_run(terminal_t *term) {
    if (!term || !term->port || !term->port->is_open) return -1;

    struct termios orig_termios;
    if (set_raw_mode(&orig_termios) < 0) {
        fprintf(stderr, "Warning: could not set raw mode, using default\n");
    }

    terminal_help();

    char recv_buf[4096];
    unsigned char ch;

    while (term->running) {
        fd_set readfds;
        struct timeval tv;
        int max_fd = term->port->fd;
        int stdin_fd = STDIN_FILENO;

        FD_ZERO(&readfds);

        /* Watch both serial port and stdin */
        FD_SET(term->port->fd, &readfds);
        if (isatty(stdin_fd)) {
            FD_SET(stdin_fd, &readfds);
            if (stdin_fd > max_fd) max_fd = stdin_fd;
        }

        /* Short timeout for responsiveness */
        tv.tv_sec  = 0;
        tv.tv_usec = 100000; /* 100ms */

        int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        /* Check for data from serial port */
        if (FD_ISSET(term->port->fd, &readfds)) {
            int n = serial_recv(term->port, recv_buf, sizeof(recv_buf), 0);
            if (n > 0) {
                display_data(term->port, recv_buf, n);
            } else if (n < 0) {
                break; /* Port error */
            }
        }

        /* Check for keyboard input */
        if (isatty(stdin_fd) && FD_ISSET(stdin_fd, &readfds)) {
            if (read(stdin_fd, &ch, 1) != 1) continue;

            /* Handle escape sequence (Ctrl+A prefix) */
            if (term->escape_mode) {
                term->escape_mode = 0;

                switch (ch) {
                    case '?':
                        terminal_help();
                        break;
                    case 'q':
                    case 'Q':
                        printf("\nExiting terminal...\n");
                        term->running = 0;
                        break;
                    case 'h':
                    case 'H':
                        term->port->hex_mode = !term->port->hex_mode;
                        printf("\nHex mode: %s\n",
                               term->port->hex_mode ? "ON" : "OFF");
                        break;
                    case 't':
                    case 'T':
                        term->port->timestamp = !term->port->timestamp;
                        printf("\nTimestamps: %s\n",
                               term->port->timestamp ? "ON" : "OFF");
                        break;
                    case 'e':
                    case 'E':
                        term->local_echo = !term->local_echo;
                        printf("\nLocal echo: %s\n",
                               term->local_echo ? "ON" : "OFF");
                        break;
                    case 'l':
                    case 'L':
                        term->line_mode = !term->line_mode;
                        printf("\nLine mode: %s\n",
                               term->line_mode ? "ON" : "OFF");
                        break;
                    case 's':
                    case 'S': {
                        /* Prompt for filename */
                        struct termios saved;
                        tcgetattr(STDIN_FILENO, &saved);
                        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

                        printf("\nFile to send: ");
                        fflush(stdout);

                        char filename[256];
                        if (fgets(filename, sizeof(filename), stdin)) {
                            /* Remove trailing newline */
                            size_t len = strlen(filename);
                            while (len > 0 && (filename[len-1] == '\n' ||
                                               filename[len-1] == '\r')) {
                                filename[--len] = '\0';
                            }
                            if (len > 0) {
                                send_file(term->port, filename);
                            }
                        }

                        set_raw_mode(&saved);
                        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
                        break;
                    }
                    case 'c':
                    case 'C': {
                        /* Send Ctrl+C */
                        unsigned char ctrl_c = 0x03;
                        serial_send(term->port, &ctrl_c, 1);
                        if (term->local_echo) printf("^C");
                        break;
                    }
                    case 'd':
                    case 'D': {
                        /* Send Ctrl+D */
                        unsigned char ctrl_d = 0x04;
                        serial_send(term->port, &ctrl_d, 1);
                        if (term->local_echo) printf("^D");
                        break;
                    }
                    case 'a':
                    case 'A':
                        /* Send literal Ctrl+A */
                        serial_send(term->port, &ch, 1);
                        break;
                    default:
                        printf("\nUnknown command: Ctrl+A %c\n", ch);
                        terminal_help();
                        break;
                }
                continue;
            }

            /* Check for Ctrl+A */
            if (ch == 0x01) {
                term->escape_mode = 1;
                continue;
            }

            /* In line mode, buffer input until Enter */
            if (term->line_mode) {
                static char line_buf[1024];
                static int line_pos = 0;

                if (ch == '\r' || ch == '\n') {
                    /* Send the line */
                    line_buf[line_pos] = '\n';
                    serial_send(term->port, line_buf, line_pos + 1);
                    if (term->local_echo) printf("\r\n");
                    line_pos = 0;
                } else if (ch == 0x7F || ch == 0x08) {
                    /* Backspace */
                    if (line_pos > 0) {
                        line_pos--;
                        if (term->local_echo) printf("\b \b");
                    }
                } else if (line_pos < (int)sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = ch;
                    if (term->local_echo) putchar(ch);
                }
                fflush(stdout);
            } else {
                /* Raw character mode - send immediately */
                serial_send(term->port, &ch, 1);
                if (term->local_echo) {
                    if (ch == '\r') printf("\r\n");
                    else if (ch < 0x20) printf("^%c", ch + '@');
                    else putchar(ch);
                    fflush(stdout);
                }
            }
        }
    }

    /* Restore terminal settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    return 0;
}
