#include "hexdump.h"
#include <stdio.h>
#include <ctype.h>
#include <time.h>

void hexdump_print(const void *data, size_t len, unsigned long offset) {
    if (!data || len == 0) return;

    const unsigned char *ptr = (const unsigned char *)data;
    size_t i;

    for (i = 0; i < len; i += 16) {
        /* Print offset */
        printf("%08lx: ", offset + i);

        /* Print hex bytes */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                printf("%02x ", ptr[i + j]);
            } else {
                printf("   ");
            }
            /* Extra space after 8th byte for readability */
            if (j == 7) printf(" ");
        }

        /* Print ASCII representation */
        printf(" |");
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            unsigned char c = ptr[i + j];
            putchar(isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}

void hexdump_timestamp(const void *data, size_t len) {
    if (!data || len == 0) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_info);

    const unsigned char *ptr = (const unsigned char *)data;

    /* Print each byte with timestamp prefix */
    printf("[%s.%03ld] ", time_buf, ts.tv_nsec / 1000000);

    for (size_t i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            printf("\n");
            if (i + 1 < len) {
                /* Print timestamp for next line */
                clock_gettime(CLOCK_REALTIME, &ts);
                localtime_r(&ts.tv_sec, &tm_info);
                strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_info);
                printf("[%s.%03ld] ", time_buf, ts.tv_nsec / 1000000);
            }
        } else if (ptr[i] == '\r') {
            /* Skip CR */
        } else if (isprint(ptr[i])) {
            putchar(ptr[i]);
        } else {
            printf("\\x%02x", ptr[i]);
        }
    }
    fflush(stdout);
}
