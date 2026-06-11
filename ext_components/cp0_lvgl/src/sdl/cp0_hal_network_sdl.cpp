#include "hal/hal_network.h"
#include <string.h>

#ifdef _WIN32

int hal_network_list(hal_netif_info_t *entries, int max_entries, int *out_count)
{
    (void)entries; (void)max_entries;
    *out_count = 0;
    return 0;
}

#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>

int hal_network_list(hal_netif_info_t *entries, int max_entries, int *out_count)
{
    *out_count = 0;
    struct ifaddrs *ifap = NULL;
    if (getifaddrs(&ifap) != 0) return -1;

    for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0 || strcmp(ifa->ifa_name, "lo0") == 0) continue;
        if (*out_count >= max_entries) break;

        hal_netif_info_t *e = &entries[*out_count];
        strncpy(e->iface, ifa->ifa_name, 31); e->iface[31] = '\0';
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, e->ipv4, sizeof(e->ipv4));
        if (ifa->ifa_netmask) {
            struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
            inet_ntop(AF_INET, &nm->sin_addr, e->netmask, sizeof(e->netmask));
        } else {
            strcpy(e->netmask, "N/A");
        }
        e->is_up = (ifa->ifa_flags & IFF_UP) ? 1 : 0;
        (*out_count)++;
    }
    freeifaddrs(ifap);
    return 0;
}

#endif
