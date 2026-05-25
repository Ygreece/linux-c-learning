#include "sysmon.h"

int disk_read(sys_info_t *sys) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return -1;

    char line[1024];
    sys->num_disks = 0;

    while (fgets(line, sizeof(line), f) && sys->num_disks < MAX_MOUNTS) {
        char device[64], mount[128], fs_type[32], opts[256];
        int dump, pass;

        if (sscanf(line, "%63s %127s %31s %255s %d %d",
                   device, mount, fs_type, opts, &dump, &pass) != 6) {
            continue;
        }

        /* Skip pseudo-filesystems */
        if (strcmp(fs_type, "proc") == 0 ||
            strcmp(fs_type, "sysfs") == 0 ||
            strcmp(fs_type, "devpts") == 0 ||
            strcmp(fs_type, "tmpfs") == 0 ||
            strcmp(fs_type, "devtmpfs") == 0 ||
            strcmp(fs_type, "cgroup") == 0 ||
            strcmp(fs_type, "cgroup2") == 0 ||
            strcmp(fs_type, "pstore") == 0 ||
            strcmp(fs_type, "securityfs") == 0 ||
            strcmp(fs_type, "debugfs") == 0 ||
            strcmp(fs_type, "tracefs") == 0 ||
            strcmp(fs_type, "fusectl") == 0 ||
            strcmp(fs_type, "configfs") == 0 ||
            strcmp(fs_type, "hugetlbfs") == 0 ||
            strcmp(fs_type, "mqueue") == 0 ||
            strcmp(fs_type, "binfmt_misc") == 0 ||
            strcmp(fs_type, "autofs") == 0 ||
            strncmp(device, "/dev/loop", 9) == 0) {
            continue;
        }

        /* Get disk usage via statvfs */
        struct statvfs vfs;
        if (statvfs(mount, &vfs) != 0) continue;

        disk_info_t *disk = &sys->disks[sys->num_disks];
        snprintf(disk->device, sizeof(disk->device), "%s", device);
        snprintf(disk->mount, sizeof(disk->mount), "%s", mount);
        snprintf(disk->fs_type, sizeof(disk->fs_type), "%s", fs_type);

        unsigned long long block_size = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
        disk->total = (unsigned long long)vfs.f_blocks * block_size;
        disk->available = (unsigned long long)vfs.f_bavail * block_size;
        unsigned long long free_space = (unsigned long long)vfs.f_bfree * block_size;
        disk->used = disk->total - free_space;

        if (disk->total > 0) {
            disk->used_percent = 100.0 * (double)disk->used / (double)disk->total;
        } else {
            disk->used_percent = 0.0;
        }

        sys->num_disks++;
    }

    fclose(f);
    return 0;
}
