#pragma once

#include <cstddef>
#include <cstring>

template <typename Descriptor>
bool app_registry_descriptor_ids_unique(const Descriptor *entries, std::size_t count)
{
    if (count > 0 && !entries) return false;
    for (std::size_t i = 0; i < count; ++i) {
        const char *id = entries[i].config_key;
        if (!id || !id[0]) return false;
        for (std::size_t j = i + 1; j < count; ++j) {
            const char *other = entries[j].config_key;
            if (!other || !other[0] || std::strcmp(id, other) == 0) return false;
        }
    }
    return true;
}
