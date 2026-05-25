/**
 * test_log.c - 日志系统测试
 */

#include "log.h"
#include <assert.h>
#include <stdio.h>

void test_log_init(void)
{
    assert(log_init(NULL, LOG_DEBUG) == 0);
    log_close();
}

void test_log_levels(void)
{
    log_init(NULL, LOG_DEBUG);
    log_debug("debug message test");
    log_info("info message test");
    log_warn("warn message test");
    log_error("error message test");
    log_fatal("fatal message test");
    log_close();
}

void test_log_file_output(void)
{
    assert(log_init("./log", LOG_INFO) == 0);
    log_info("file output test");
    log_close();
}

int main(void)
{
    test_log_init();
    test_log_levels();
    test_log_file_output();
    printf("All log tests passed!\n");
    return 0;
}
