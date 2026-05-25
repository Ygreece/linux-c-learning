#ifndef HEXDUMP_H
#define HEXDUMP_H

#include <stddef.h>

/* Print hex dump of data */
void hexdump_print(const void *data, size_t len, unsigned long offset);

/* Print data with timestamp */
void hexdump_timestamp(const void *data, size_t len);

#endif
