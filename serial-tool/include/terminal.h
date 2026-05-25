#ifndef TERMINAL_H
#define TERMINAL_H

#include "serial.h"

typedef struct {
    serial_port_t *port;
    int running;
    int escape_mode;     /* 1 when Ctrl+A pressed */
    int local_echo;
    int line_mode;       /* 1 = line by line, 0 = raw char */
} terminal_t;

/* Start interactive terminal session */
int terminal_run(terminal_t *term);

/* Print help for terminal mode */
void terminal_help(void);

#endif
