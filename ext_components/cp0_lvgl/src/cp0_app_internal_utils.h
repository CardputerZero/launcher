#pragma once

#include <cstdio>
#include <string>

static inline void cp0_copy_string(char *dst, int dst_size, const std::string &src)
{
    if (!dst || dst_size <= 0)
        return;
    snprintf(dst, static_cast<size_t>(dst_size), "%s", src.c_str());
}

static inline void cp0_copy_cstr(char *dst, int dst_size, const char *src)
{
    if (!dst || dst_size <= 0)
        return;
    snprintf(dst, static_cast<size_t>(dst_size), "%s", src ? src : "");
}
