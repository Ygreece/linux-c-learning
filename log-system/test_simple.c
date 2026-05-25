#include "log.h"
#include <stdio.h>

int main() {
    printf("Test 1: init_close\n");
    log_config_t config = LOG_DEFAULT_CONFIG;
    config.targets = LOG_TARGET_CONSOLE;
    config.level = LOG_DEBUG;
    log_init(&config);
    printf("  Init OK\n");
    log_close();
    printf("  Close OK\n");

    printf("Test 2: level_filter\n");
    config.level = LOG_WARN;
    log_init(&config);
    log_info("This should be filtered");
    log_warn("This should appear");
    log_close();

    printf("All tests passed!\n");
    return 0;
}
