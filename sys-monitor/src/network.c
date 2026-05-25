#include "sysmon.h"

int network_read(sys_info_t *sys) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return -1;

    char line[512];
    sys->num_net_ifaces = 0;

    /* Skip header lines */
    if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof(line), f) && sys->num_net_ifaces < MAX_NET_IFACES) {
        net_info_t *net = &sys->net[sys->num_net_ifaces];

        /* Parse interface name and stats */
        /* Format: "iface: rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame rx_compressed rx_multicast tx_bytes ..." */
        char *colon = strchr(line, ':');
        if (!colon) continue;

        /* Extract interface name */
        *colon = '\0';
        char *name_start = line;
        while (*name_start == ' ') name_start++;
        name_start[sizeof(net->name) - 1] = '\0';
        snprintf(net->name, sizeof(net->name), "%s", name_start);
        *colon = ':';

        /* Skip loopback */
        if (strcmp(net->name, "lo") == 0) continue;

        /* Parse stats after the colon */
        unsigned long long rx_bytes, tx_bytes;
        unsigned long long rx_packets, tx_packets;
        unsigned long long dummy;

        int ret = sscanf(colon + 1,
            "%llu %llu %llu %llu %llu %llu %llu %llu "
            "%llu %llu %llu %llu %llu %llu %llu %llu",
            &rx_bytes, &rx_packets, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
            &tx_bytes, &tx_packets, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy);

        if (ret < 10) continue;

        net->rx_bytes = rx_bytes;
        net->tx_bytes = tx_bytes;
        net->rx_packets = rx_packets;
        net->tx_packets = tx_packets;
        net->rx_rate = 0;
        net->tx_rate = 0;

        sys->num_net_ifaces++;
    }

    fclose(f);
    return 0;
}

void network_calculate_rate(sys_info_t *sys, const sys_info_t *prev, double elapsed) {
    if (elapsed <= 0.0) return;

    for (int i = 0; i < sys->num_net_ifaces; i++) {
        /* Find matching previous interface */
        for (int j = 0; j < prev->num_net_ifaces; j++) {
            if (strcmp(sys->net[i].name, prev->net[j].name) == 0) {
                unsigned long long rx_diff = sys->net[i].rx_bytes - prev->net[j].rx_bytes;
                unsigned long long tx_diff = sys->net[i].tx_bytes - prev->net[j].tx_bytes;
                sys->net[i].rx_rate = (unsigned long)(rx_diff / elapsed);
                sys->net[i].tx_rate = (unsigned long)(tx_diff / elapsed);
                break;
            }
        }
    }
}
