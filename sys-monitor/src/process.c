#include "sysmon.h"
#include <pwd.h>

/* Get username from UID */
static void get_username(uid_t uid, char *buf, size_t buflen) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        snprintf(buf, buflen, "%s", pw->pw_name);
    } else {
        snprintf(buf, buflen, "%u", uid);
    }
}

int process_read(sys_info_t *sys) {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) return -1;

    struct dirent *entry;
    sys->num_processes = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    long clock_ticks = sysconf(_SC_CLK_TCK);
    unsigned long long total_mem_kb = sys->memory.total;

    while ((entry = readdir(proc_dir)) != NULL && sys->num_processes < MAX_PROCESSES) {
        /* Skip non-numeric entries */
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid <= 0) continue;

        char path[512];
        proc_info_t *proc = &sys->processes[sys->num_processes];
        memset(proc, 0, sizeof(*proc));
        proc->pid = pid;

        /* Read /proc/[pid]/stat */
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char comm[256];
        int nice_val = 0;
        unsigned long utime = 0, stime = 0;
        unsigned long vsize = 0;
        long rss = 0;

        /* Parse the stat file - comm field can contain spaces and parens */
        char stat_line[1024];
        if (!fgets(stat_line, sizeof(stat_line), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        /* Find the last ')' to handle comm fields with special chars */
        char *rparen = strrchr(stat_line, ')');
        if (!rparen) continue;

        /* Parse pid and comm from the beginning */
        sscanf(stat_line, "%d (%255[^)])", &proc->pid, comm);
        comm[sizeof(proc->name) - 1] = '\0';
        snprintf(proc->name, sizeof(proc->name), "%s", comm);

        /* Parse fields after comm: state ppid ... utime stime ... nice ... vsize rss ... */
        /* Format after "): state ppid pgroup session tty_nr tpgid flags minflt cminflt majflt
         * cmajflt utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss */

        /* We need to parse specific fields by position */
        /* Fields after "): " are separated by spaces */
        /* We need: field 3(ppid), 14(utime), 15(stime), 19(nice), 23(vsize), 24(rss) */
        int field = 0;
        char *token = strtok(rparen + 2, " ");
        while (token && field < 39) {
            field++;
            switch (field) {
                case 1: proc->state = token[0]; break;
                case 2: proc->ppid = (pid_t)atoi(token); break;
                case 14: utime = strtoul(token, NULL, 10); break;
                case 15: stime = strtoul(token, NULL, 10); break;
                case 18: nice_val = atoi(token); break;
                case 23: vsize = strtoul(token, NULL, 10); break;
                case 24: rss = strtol(token, NULL, 10); break;
            }
            token = strtok(NULL, " ");
        }

        proc->nice = nice_val;
        proc->vm_size = vsize / 1024; /* Convert to kB */
        proc->vm_rss = (unsigned long)rss * page_size / 1024; /* Convert pages to kB */

        /* Calculate CPU percent: (utime + stime) in clock ticks, relative to total CPU time */
        /* We approximate: cpu% = (utime+stime) / (uptime * clock_ticks) * 100 */
        if (sys->uptime > 0 && clock_ticks > 0) {
            double total_time = (double)(utime + stime) / (double)clock_ticks;
            proc->cpu_percent = 100.0 * total_time / sys->uptime;
        }

        /* Calculate memory percent */
        if (total_mem_kb > 0) {
            proc->mem_percent = 100.0 * (double)proc->vm_rss / (double)total_mem_kb;
        }

        /* Read /proc/[pid]/status for UID */
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        f = fopen(path, "r");
        if (f) {
            char status_line[256];
            while (fgets(status_line, sizeof(status_line), f)) {
                unsigned int uid = 0;
                if (sscanf(status_line, "Uid:\t%u", &uid) == 1) {
                    get_username(uid, proc->user, sizeof(proc->user));
                    break;
                }
            }
            fclose(f);
        }

        /* Read command line */
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        f = fopen(path, "r");
        if (f) {
            size_t nread = fread(proc->command, 1, sizeof(proc->command) - 1, f);
            proc->command[nread] = '\0';
            /* Replace null separators with spaces */
            for (size_t i = 0; i < nread; i++) {
                if (proc->command[i] == '\0') proc->command[i] = ' ';
            }
            fclose(f);
        }

        /* If command is empty, use the name from stat */
        if (proc->command[0] == '\0') {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "[%s]", proc->name);
            snprintf(proc->command, sizeof(proc->command), "%s", tmp);
        }

        sys->num_processes++;
    }

    closedir(proc_dir);

    /* Sort by CPU usage (descending) */
    qsort(sys->processes, sys->num_processes, sizeof(proc_info_t), process_compare_cpu);

    return 0;
}

int process_compare_cpu(const void *a, const void *b) {
    const proc_info_t *pa = (const proc_info_t *)a;
    const proc_info_t *pb = (const proc_info_t *)b;
    if (pb->cpu_percent > pa->cpu_percent) return 1;
    if (pb->cpu_percent < pa->cpu_percent) return -1;
    return 0;
}

int process_compare_mem(const void *a, const void *b) {
    const proc_info_t *pa = (const proc_info_t *)a;
    const proc_info_t *pb = (const proc_info_t *)b;
    if (pb->mem_percent > pa->mem_percent) return 1;
    if (pb->mem_percent < pa->mem_percent) return -1;
    return 0;
}
