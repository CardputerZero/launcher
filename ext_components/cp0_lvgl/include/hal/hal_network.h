#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char iface[32];
    char ipv4[16];
    char netmask[16];
    int  is_up;
} hal_netif_info_t;

int hal_network_list(hal_netif_info_t *entries, int max_entries,
                     int *out_count);

#ifdef __cplusplus
}
#endif
