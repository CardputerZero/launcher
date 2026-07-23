#include "cp0_lora_device_discovery.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>

namespace cp0_lora_device_discovery {

size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred)
{
    if (out == nullptr || max_count == 0) return 0;

    size_t count = 0;
    auto append_candidate = [&](const char *path) {
        if (path == nullptr || path[0] == '\0') return;
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(out[i], path) == 0) return;
        }
        if (count < max_count) {
            snprintf(out[count], 64, "%s", path);
            ++count;
        }
    };

    append_candidate(preferred);
    append_candidate("/dev/spidev0.1");
    append_candidate("/dev/spidev0.0");

    DIR *dir = opendir("/dev");
    if (dir != nullptr) {
        while (dirent *entry = readdir(dir)) {
            if (strncmp(entry->d_name, "spidev", 6) != 0) continue;
            char full_path[64];
            constexpr char dev_prefix[] = "/dev/";
            const size_t prefix_len = strlen(dev_prefix);
            const size_t name_len = strlen(entry->d_name);
            if (name_len >= sizeof(full_path) - prefix_len) continue;
            memcpy(full_path, dev_prefix, prefix_len);
            memcpy(full_path + prefix_len, entry->d_name, name_len + 1);
            append_candidate(full_path);
        }
        closedir(dir);
    }

    static const char *fallbacks[] = {
        "/dev/spidev0.1", "/dev/spidev0.0", "/dev/spidev1.0", "/dev/spidev1.1",
        "/dev/spidev2.0", "/dev/spidev2.1", "/dev/spidev3.0", "/dev/spidev3.1",
        "/dev/spidev4.0", "/dev/spidev4.1",
    };
    for (const char *fallback : fallbacks) append_candidate(fallback);
    return count;
}

} // namespace cp0_lora_device_discovery
