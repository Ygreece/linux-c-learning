#include "sysmon.h"

int memory_read(sys_info_t *sys) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    char line[256];
    mem_info_t *mem = &sys->memory;

    /* Zero out fields before reading */
    mem->total = 0;
    mem->free = 0;
    mem->available = 0;
    mem->buffers = 0;
    mem->cached = 0;
    mem->swap_total = 0;
    mem->swap_free = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &mem->total) == 1) continue;
        if (sscanf(line, "MemFree: %llu kB", &mem->free) == 1) continue;
        if (sscanf(line, "MemAvailable: %llu kB", &mem->available) == 1) continue;
        if (sscanf(line, "Buffers: %llu kB", &mem->buffers) == 1) continue;
        if (sscanf(line, "Cached: %llu kB", &mem->cached) == 1) continue;
        if (sscanf(line, "SwapTotal: %llu kB", &mem->swap_total) == 1) continue;
        if (sscanf(line, "SwapFree: %llu kB", &mem->swap_free) == 1) continue;
    }

    fclose(f);

    /* Calculate used percentage */
    if (mem->total > 0) {
        unsigned long long used = mem->total - mem->available;
        mem->used_percent = 100.0 * (double)used / (double)mem->total;
    }

    return 0;
}
