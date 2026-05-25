#ifndef SYSMON_H
#define SYSMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <dirent.h>
#include <time.h>
#include <sys/statvfs.h>
#include <math.h>

#define MAX_CPUS 32
#define MAX_PROCESSES 1024
#define MAX_MOUNTS 16
#define MAX_NET_IFACES 16

/* CPU info */
typedef struct {
    char name[16];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long total;
    double usage_percent;
} cpu_info_t;

/* Memory info */
typedef struct {
    unsigned long long total;       /* kB */
    unsigned long long free;
    unsigned long long available;
    unsigned long long buffers;
    unsigned long long cached;
    unsigned long long swap_total;
    unsigned long long swap_free;
    double used_percent;
} mem_info_t;

/* Process info */
typedef struct {
    pid_t pid;
    pid_t ppid;
    char user[32];
    char state;
    unsigned long vm_rss;     /* kB */
    unsigned long vm_size;    /* kB */
    double cpu_percent;
    double mem_percent;
    char command[256];
    char name[64];
    int nice;
} proc_info_t;

/* Disk info */
typedef struct {
    char device[64];
    char mount[128];
    char fs_type[32];
    unsigned long long total;    /* bytes */
    unsigned long long used;
    unsigned long long available;
    double used_percent;
} disk_info_t;

/* Network info */
typedef struct {
    char name[32];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    unsigned long long rx_packets;
    unsigned long long tx_packets;
    unsigned long rx_rate;    /* bytes/sec */
    unsigned long tx_rate;
} net_info_t;

/* System info */
typedef struct {
    char hostname[128];
    char kernel[128];
    double uptime;           /* seconds */
    double loadavg[3];       /* 1, 5, 15 min */
    int num_cpus;
    cpu_info_t cpus[MAX_CPUS];
    cpu_info_t cpu_total;
    mem_info_t memory;
    int num_processes;
    proc_info_t processes[MAX_PROCESSES];
    int num_disks;
    disk_info_t disks[MAX_MOUNTS];
    int num_net_ifaces;
    net_info_t net[MAX_NET_IFACES];
} sys_info_t;

/* cpu.c */
int cpu_read(sys_info_t *sys);
void cpu_calculate_usage(sys_info_t *sys, const sys_info_t *prev);

/* memory.c */
int memory_read(sys_info_t *sys);

/* process.c */
int process_read(sys_info_t *sys);
int process_compare_cpu(const void *a, const void *b);
int process_compare_mem(const void *a, const void *b);

/* disk.c */
int disk_read(sys_info_t *sys);

/* network.c */
int network_read(sys_info_t *sys);
void network_calculate_rate(sys_info_t *sys, const sys_info_t *prev, double elapsed);

/* ui.c */
int ui_init(void);
void ui_cleanup(void);
void ui_draw(const sys_info_t *sys, int selected_tab, int scroll_offset);
int ui_handle_input(int *selected_tab, int *scroll_offset);
void ui_draw_cpu(const sys_info_t *sys, int y);
void ui_draw_memory(const sys_info_t *sys, int y);
void ui_draw_processes(const sys_info_t *sys, int y, int scroll_offset);
void ui_draw_disks(const sys_info_t *sys, int y);
void ui_draw_network(const sys_info_t *sys, int y);
void ui_draw_bar(int y, int x, int width, double percent, int color_pair);

#endif
