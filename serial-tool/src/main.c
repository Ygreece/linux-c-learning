#include "serial.h"
#include "terminal.h"
#include "hexdump.h"
#include "script.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\nSerial port debugging tool for embedded development.\n\n");
    printf("Options:\n");
    printf("  -d, --device <dev>     Serial device (default: /dev/ttyUSB0)\n");
    printf("  -b, --baud <rate>      Baud rate (default: 115200)\n");
    printf("  -D, --databits <n>     Data bits: 5,6,7,8 (default: 8)\n");
    printf("  -s, --stopbits <n>     Stop bits: 1,2 (default: 1)\n");
    printf("  -p, --parity <type>    Parity: N,E,O (default: N)\n");
    printf("  -x, --hex              Hex mode (display as hex)\n");
    printf("  -t, --timestamp        Show timestamps\n");
    printf("  -l, --log <file>       Log to file\n");
    printf("  -e, --execute <file>   Run script file\n");
    printf("  -S, --send <data>      Send data and exit\n");
    printf("  -L, --list             List available serial ports\n");
    printf("  -h, --help             Show this help\n");
    printf("\nInteractive mode keys:\n");
    printf("  Ctrl+A then ?    - Show help\n");
    printf("  Ctrl+A then q    - Quit\n");
    printf("  Ctrl+A then h    - Toggle hex mode\n");
    printf("  Ctrl+A then t    - Toggle timestamps\n");
    printf("  Ctrl+A then s    - Send file\n");
}

int main(int argc, char *argv[]) {
    const char *device = "/dev/ttyUSB0";
    int baud = 115200;
    int databits = 8;
    int stopbits = 1;
    char parity = 'N';
    int hex_mode = 0;
    int timestamp = 0;
    const char *log_file = NULL;
    const char *script_file = NULL;
    const char *send_data = NULL;
    int list_ports = 0;

    static struct option long_options[] = {
        {"device",    required_argument, 0, 'd'},
        {"baud",      required_argument, 0, 'b'},
        {"databits",  required_argument, 0, 'D'},
        {"stopbits",  required_argument, 0, 's'},
        {"parity",    required_argument, 0, 'p'},
        {"hex",       no_argument,       0, 'x'},
        {"timestamp", no_argument,       0, 't'},
        {"log",       required_argument, 0, 'l'},
        {"execute",   required_argument, 0, 'e'},
        {"send",      required_argument, 0, 'S'},
        {"list",      no_argument,       0, 'L'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:b:D:s:p:xtl:e:S:Lh",
                              long_options, NULL)) != -1) {
        switch (opt) {
            case 'd': device = optarg; break;
            case 'b': baud = atoi(optarg); break;
            case 'D': databits = atoi(optarg); break;
            case 's': stopbits = atoi(optarg); break;
            case 'p': parity = optarg[0]; break;
            case 'x': hex_mode = 1; break;
            case 't': timestamp = 1; break;
            case 'l': log_file = optarg; break;
            case 'e': script_file = optarg; break;
            case 'S': send_data = optarg; break;
            case 'L': list_ports = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (list_ports) {
        char ports[32][256];
        int count = serial_list_ports(ports, 32);
        printf("Available serial ports:\n");
        for (int i = 0; i < count; i++) {
            printf("  %s\n", ports[i]);
        }
        return 0;
    }

    /* Open serial port */
    serial_port_t port;
    memset(&port, 0, sizeof(port));

    if (serial_open(&port, device, baud) != 0) {
        fprintf(stderr, "Failed to open %s\n", device);
        return 1;
    }

    serial_configure(&port, baud, databits, stopbits, parity);
    port.hex_mode = hex_mode;
    port.timestamp = timestamp;
    port.log_fd = -1;

    /* Open log file if specified */
    if (log_file) {
        port.log_fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (port.log_fd < 0) {
            fprintf(stderr, "Warning: could not open log file %s\n", log_file);
        }
    }

    printf("Connected to %s at %d baud (%d%c%d)\n",
           device, baud, databits, parity, stopbits);

    if (send_data) {
        /* Send mode */
        serial_send_str(&port, send_data);
        usleep(100000); /* Wait for response */
        char buf[4096];
        int n = serial_recv(&port, buf, sizeof(buf), 1000);
        if (n > 0) {
            if (hex_mode) hexdump_print(buf, n, 0);
            else fwrite(buf, 1, n, stdout);
            fflush(stdout);
        }
    } else if (script_file) {
        /* Script mode */
        script_run(&port, script_file);
    } else {
        /* Interactive terminal mode */
        terminal_t term = {
            .port = &port,
            .running = 1,
            .escape_mode = 0,
            .local_echo = 0,
            .line_mode = 0
        };
        terminal_run(&term);
    }

    serial_close(&port);
    return 0;
}
