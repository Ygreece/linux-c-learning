#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

/* Trim leading and trailing whitespace */
static char *trim(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
    return str;
}

/* Extract a quoted string argument: "text" -> text */
static int extract_quoted(const char *line, char *buf, size_t buf_size) {
    const char *start = strchr(line, '"');
    if (!start) return -1;
    start++;

    const char *end = strchr(start, '"');
    if (!end) return -1;

    size_t len = (size_t)(end - start);
    if (len >= buf_size) len = buf_size - 1;

    memcpy(buf, start, len);
    buf[len] = '\0';
    return (int)len;
}

int script_send_recv(serial_port_t *port, const char *send,
                     char *recv_buf, size_t recv_size,
                     const char *expected, int timeout_ms) {
    if (!port) return -1;

    /* Send data */
    if (send && send[0]) {
        int ret = serial_send_str(port, send);
        if (ret < 0) {
            fprintf(stderr, "Script: send failed\n");
            return -1;
        }
    }

    if (!recv_buf || recv_size == 0) return 0;

    /* Receive with timeout, collecting response */
    size_t total = 0;
    int elapsed = 0;
    int step = 100; /* Check every 100ms */

    while (elapsed < timeout_ms && total < recv_size - 1) {
        int n = serial_recv(port, recv_buf + total, recv_size - 1 - total, step);
        if (n > 0) {
            total += (size_t)n;
            recv_buf[total] = '\0';

            /* Check if expected pattern found */
            if (expected && expected[0]) {
                if (strstr(recv_buf, expected) != NULL) {
                    return (int)total;
                }
            }
        }
        elapsed += step;
    }

    recv_buf[total] = '\0';

    /* If we expected something but didn't find it */
    if (expected && expected[0] && strstr(recv_buf, expected) == NULL) {
        fprintf(stderr, "Script: expected \"%s\" but got: %s\n",
                expected, recv_buf);
        return -2; /* Pattern mismatch */
    }

    return (int)total;
}

int script_run(serial_port_t *port, const char *script_file) {
    if (!port || !script_file) return -1;

    FILE *fp = fopen(script_file, "r");
    if (!fp) {
        fprintf(stderr, "Script: cannot open %s\n", script_file);
        return -1;
    }

    printf("Running script: %s\n", script_file);

    char line[4096];
    int line_num = 0;
    int errors = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char *trimmed = trim(line);

        /* Skip empty lines and comments */
        if (!trimmed[0] || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        printf("[%d] %s\n", line_num, trimmed);

        /* Parse command */
        if (strncmp(trimmed, "send ", 5) == 0) {
            /* Send string: send "text" */
            char text[2048];
            if (extract_quoted(trimmed + 5, text, sizeof(text)) >= 0) {
                /* Process escape sequences */
                size_t len = strlen(text);
                /* Convert \n, \r, \t */
                char processed[2048];
                size_t j = 0;
                for (size_t i = 0; i < len && j < sizeof(processed) - 1; i++) {
                    if (text[i] == '\\' && i + 1 < len) {
                        switch (text[i + 1]) {
                            case 'n':  processed[j++] = '\n'; i++; break;
                            case 'r':  processed[j++] = '\r'; i++; break;
                            case 't':  processed[j++] = '\t'; i++; break;
                            case '\\': processed[j++] = '\\'; i++; break;
                            case '"':  processed[j++] = '"';  i++; break;
                            default:   processed[j++] = text[i]; break;
                        }
                    } else {
                        processed[j++] = text[i];
                    }
                }
                processed[j] = '\0';

                int ret = serial_send_str(port, processed);
                if (ret < 0) {
                    fprintf(stderr, "  ERROR: send failed\n");
                    errors++;
                } else {
                    printf("  -> sent %d bytes\n", ret);
                }
            } else {
                /* Try sending without quotes */
                int ret = serial_send_str(port, trimmed + 5);
                if (ret < 0) {
                    fprintf(stderr, "  ERROR: send failed\n");
                    errors++;
                }
            }
        } else if (strncmp(trimmed, "send_hex ", 9) == 0) {
            /* Send hex bytes: send_hex "FF 01 A3" */
            char hex[2048];
            if (extract_quoted(trimmed + 9, hex, sizeof(hex)) >= 0) {
                int ret = serial_send_hex(port, hex);
                if (ret < 0) {
                    fprintf(stderr, "  ERROR: send_hex failed\n");
                    errors++;
                } else {
                    printf("  -> sent %d bytes\n", ret);
                }
            } else {
                /* Try without quotes */
                int ret = serial_send_hex(port, trimmed + 9);
                if (ret < 0) {
                    fprintf(stderr, "  ERROR: send_hex failed\n");
                    errors++;
                }
            }
        } else if (strncmp(trimmed, "expect ", 7) == 0) {
            /* Expect pattern: expect "pattern" [timeout] */
            char pattern[2048];
            if (extract_quoted(trimmed + 7, pattern, sizeof(pattern)) >= 0) {
                /* Check for optional timeout */
                const char *after = strchr(trimmed + 7, '"');
                after = after ? strchr(after + 1, '"') : NULL;
                int timeout = 5000; /* Default 5 seconds */
                if (after) {
                    after++;
                    while (*after && isspace((unsigned char)*after)) after++;
                    if (*after) timeout = atoi(after);
                }

                char recv_buf[4096];
                int n = script_send_recv(port, NULL, recv_buf, sizeof(recv_buf),
                                         pattern, timeout);
                if (n == -2) {
                    fprintf(stderr, "  FAIL: pattern not found\n");
                    errors++;
                } else if (n < 0) {
                    fprintf(stderr, "  ERROR: receive failed\n");
                    errors++;
                } else {
                    printf("  <- received %d bytes, pattern matched\n", n);
                }
            } else {
                fprintf(stderr, "  ERROR: invalid expect syntax\n");
                errors++;
            }
        } else if (strncmp(trimmed, "wait ", 5) == 0) {
            /* Wait milliseconds: wait 1000 */
            int ms = atoi(trimmed + 5);
            if (ms > 0) {
                printf("  ... waiting %d ms\n", ms);
                usleep((useconds_t)ms * 1000);
            }
        } else if (strncmp(trimmed, "send_recv ", 10) == 0) {
            /* Send and expect: send_recv "send" "expect" [timeout] */
            char send_text[2048];
            char expect_text[2048];
            const char *p = trimmed + 10;

            if (extract_quoted(p, send_text, sizeof(send_text)) >= 0) {
                const char *next = strchr(p, '"');
                next = next ? strchr(next + 1, '"') + 1 : NULL;

                if (next && extract_quoted(next, expect_text, sizeof(expect_text)) >= 0) {
                    int timeout = 5000;
                    const char *after = strchr(next, '"');
                    after = after ? strchr(after + 1, '"') : NULL;
                    if (after) {
                        after++;
                        while (*after && isspace((unsigned char)*after)) after++;
                        if (*after) timeout = atoi(after);
                    }

                    /* Process escape sequences in send_text */
                    char processed[2048];
                    size_t len = strlen(send_text);
                    size_t j = 0;
                    for (size_t i = 0; i < len && j < sizeof(processed) - 1; i++) {
                        if (send_text[i] == '\\' && i + 1 < len) {
                            switch (send_text[i + 1]) {
                                case 'n':  processed[j++] = '\n'; i++; break;
                                case 'r':  processed[j++] = '\r'; i++; break;
                                case 't':  processed[j++] = '\t'; i++; break;
                                case '\\': processed[j++] = '\\'; i++; break;
                                default:   processed[j++] = send_text[i]; break;
                            }
                        } else {
                            processed[j++] = send_text[i];
                        }
                    }
                    processed[j] = '\0';

                    char recv_buf[4096];
                    int n = script_send_recv(port, processed, recv_buf,
                                             sizeof(recv_buf), expect_text,
                                             timeout);
                    if (n == -2) {
                        fprintf(stderr, "  FAIL: expected \"%s\" not found\n",
                                expect_text);
                        errors++;
                    } else if (n < 0) {
                        fprintf(stderr, "  ERROR: send_recv failed\n");
                        errors++;
                    } else {
                        printf("  <- %d bytes, pattern matched\n", n);
                    }
                } else {
                    fprintf(stderr, "  ERROR: invalid send_recv syntax\n");
                    errors++;
                }
            } else {
                fprintf(stderr, "  ERROR: invalid send_recv syntax\n");
                errors++;
            }
        } else if (strncmp(trimmed, "log ", 4) == 0) {
            /* Toggle logging: log "filename" or log off */
            if (strstr(trimmed, "off") != NULL) {
                if (port->log_fd >= 0) {
                    close(port->log_fd);
                    port->log_fd = -1;
                    printf("  Logging disabled\n");
                }
            } else {
                char filename[256];
                if (extract_quoted(trimmed + 4, filename, sizeof(filename)) >= 0) {
                    if (port->log_fd >= 0) close(port->log_fd);
                    port->log_fd = open(filename,
                                        O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (port->log_fd >= 0) {
                        printf("  Logging to %s\n", filename);
                    } else {
                        fprintf(stderr, "  ERROR: cannot open log file\n");
                        errors++;
                    }
                }
            }
        } else {
            fprintf(stderr, "  WARNING: unknown command at line %d\n",
                    line_num);
        }
    }

    fclose(fp);

    printf("\nScript completed: %d error(s)\n", errors);
    return errors > 0 ? -1 : 0;
}
