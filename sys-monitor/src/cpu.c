#include "sysmon.h"

int cpu_read(sys_info_t *sys) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    sys->num_cpus = 0;
    char line[512];

    while (fgets(line, sizeof(line), f) && sys->num_cpus < MAX_CPUS) {
        if (strncmp(line, "cpu ", 4) == 0) {
            /* Total CPU */
            sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &sys->cpu_total.user, &sys->cpu_total.nice, &sys->cpu_total.system,
                   &sys->cpu_total.idle, &sys->cpu_total.iowait, &sys->cpu_total.irq,
                   &sys->cpu_total.softirq, &sys->cpu_total.steal);
            sys->cpu_total.total = sys->cpu_total.user + sys->cpu_total.nice +
                                   sys->cpu_total.system + sys->cpu_total.idle +
                                   sys->cpu_total.iowait + sys->cpu_total.irq +
                                   sys->cpu_total.softirq + sys->cpu_total.steal;
        } else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            int idx = sys->num_cpus;
            sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                   sys->cpus[idx].name,
                   &sys->cpus[idx].user, &sys->cpus[idx].nice, &sys->cpus[idx].system,
                   &sys->cpus[idx].idle, &sys->cpus[idx].iowait, &sys->cpus[idx].irq,
                   &sys->cpus[idx].softirq, &sys->cpus[idx].steal);
            sys->cpus[idx].total = sys->cpus[idx].user + sys->cpus[idx].nice +
                                    sys->cpus[idx].system + sys->cpus[idx].idle +
                                    sys->cpus[idx].iowait + sys->cpus[idx].irq +
                                    sys->cpus[idx].softirq + sys->cpus[idx].steal;
            sys->num_cpus++;
        }
    }

    fclose(f);

    /* Read load average */
    f = fopen("/proc/loadavg", "r");
    if (f) {
        if (fscanf(f, "%lf %lf %lf", &sys->loadavg[0], &sys->loadavg[1], &sys->loadavg[2]) != 3) {
            sys->loadavg[0] = sys->loadavg[1] = sys->loadavg[2] = 0.0;
        }
        fclose(f);
    }

    /* Read uptime */
    f = fopen("/proc/uptime", "r");
    if (f) {
        if (fscanf(f, "%lf", &sys->uptime) != 1) {
            sys->uptime = 0.0;
        }
        fclose(f);
    }

    return 0;
}

void cpu_calculate_usage(sys_info_t *sys, const sys_info_t *prev) {
    /* Total CPU */
    unsigned long long total_diff = sys->cpu_total.total - prev->cpu_total.total;
    unsigned long long idle_diff = sys->cpu_total.idle - prev->cpu_total.idle;
    if (total_diff > 0) {
        sys->cpu_total.usage_percent = 100.0 * (1.0 - (double)idle_diff / total_diff);
    }

    /* Per CPU */
    for (int i = 0; i < sys->num_cpus; i++) {
        unsigned long long t = sys->cpus[i].total - prev->cpus[i].total;
        unsigned long long d = sys->cpus[i].idle - prev->cpus[i].idle;
        if (t > 0) sys->cpus[i].usage_percent = 100.0 * (1.0 - (double)d / t);
    }
}
