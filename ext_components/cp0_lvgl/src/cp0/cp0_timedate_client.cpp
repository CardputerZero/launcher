#include "cp0_timedate_client.hpp"
#include <ctime>
#include <cstdio>
#if !defined(HAL_PLATFORM_SDL)
#include <gio/gio.h>
#endif

#if defined(HAL_PLATFORM_SDL)
int cp0_timedate_get_ntp(int *) { return -1; }
int cp0_timedate_get_time(int[6]) { return -1; }
int cp0_timedate_set_ntp(int) { return -1; }
int cp0_timedate_set_time(const char *) { return -1; }
#else
namespace {
constexpr const char *bus = "org.freedesktop.timedate1";
constexpr const char *path = "/org/freedesktop/timedate1";
constexpr const char *iface = "org.freedesktop.timedate1";
GDBusConnection *connection(GError **e) { return g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, e); }
GVariant *get_property(GDBusConnection *c, const char *name, GError **e) {
    return g_dbus_connection_call_sync(c, bus, path, "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", iface, name), G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, e);
}
}
int cp0_timedate_get_ntp(int *enabled) {
    if (!enabled) return -22; GError *e = nullptr; auto *c = connection(&e); if (!c) { g_clear_error(&e); return -1; }
    auto *r = get_property(c, "NTP", &e); GVariant *v = nullptr;
    if (r) g_variant_get(r, "(v)", &v);
    const bool ok = v && g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN);
    if (ok) *enabled = g_variant_get_boolean(v);
    if (v) g_variant_unref(v); if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c); return ok ? 0 : -1;
}
int cp0_timedate_get_time(int values[6]) {
    if (!values) return -22; GError *e = nullptr; auto *c = connection(&e); if (!c) { g_clear_error(&e); return -1; }
    auto *r = get_property(c, "TimeUSec", &e); GVariant *v = nullptr; guint64 us = 0;
    if (r) g_variant_get(r, "(v)", &v);
    const bool ok = v && g_variant_is_of_type(v, G_VARIANT_TYPE_UINT64);
    if (ok) us = g_variant_get_uint64(v);
    if (v) g_variant_unref(v); if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c);
    if (!ok) return -1; std::time_t t = static_cast<std::time_t>(us / 1000000ULL); std::tm tm{};
    if (!localtime_r(&t, &tm)) return -1;
    values[0] = tm.tm_year + 1900; values[1] = tm.tm_mon + 1; values[2] = tm.tm_mday; values[3] = tm.tm_hour; values[4] = tm.tm_min; values[5] = tm.tm_sec; return 0;
}
int cp0_timedate_set_ntp(int enabled) {
    GError *e = nullptr; auto *c = connection(&e); if (!c) { g_clear_error(&e); return -1; }
    auto *r = g_dbus_connection_call_sync(c, bus, path, iface, "SetNTP", g_variant_new("(bb)", enabled, false), nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &e);
    const bool ok = r != nullptr; if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c); return ok ? 0 : -1;
}
int cp0_timedate_set_time(const char *timestamp) {
    if (!timestamp) return -22; const std::string s(timestamp);
    std::tm tm{}; if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return -22;
    tm.tm_year -= 1900; --tm.tm_mon; tm.tm_isdst = -1; const auto t = std::mktime(&tm); if (t < 0) return -22;
    GError *e = nullptr; auto *c = connection(&e); if (!c) { g_clear_error(&e); return -1; }
    auto *r = g_dbus_connection_call_sync(c, bus, path, iface, "SetTime", g_variant_new("(xbb)", static_cast<gint64>(t) * 1000000, false, false), nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &e);
    const bool ok = r != nullptr; if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c); return ok ? 0 : -1;
}
#endif
