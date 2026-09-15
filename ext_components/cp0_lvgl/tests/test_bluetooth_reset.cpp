/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "../src/cp0/cp0_bluez_dbus_client.hpp"
#include <gio/gio.h>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
bool powered = true;
bool fail_off = false;
bool fail_pair = false;
int removals = 0;
std::vector<bool> power_requests;
std::vector<GDBusMethodInvocation *> held;

void method(GDBusConnection *, const gchar *, const gchar *, const gchar *,
            const gchar *name, GVariant *, GDBusMethodInvocation *call, gpointer)
{
    if (g_str_equal(name, "GetManagedObjects")) {
        GError *error = nullptr;
        auto *objects = g_variant_parse(G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
            "({'/org/bluez/hci0': {'org.bluez.Adapter1': {"
            "'Powered': <true>, 'Discoverable': <false>, 'Address': <'11:22:33:44:55:66'>}},"
            "'/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF': {'org.bluez.Device1': {"
            "'Address': <'AA:BB:CC:DD:EE:FF'>, 'Connected': <true>, 'Paired': <true>}}},)",
            nullptr, nullptr, &error);
        assert(!error && objects);
        g_dbus_method_invocation_return_value(call, objects);
    } else if (g_str_equal(name, "Pair") && fail_pair) {
        g_dbus_method_invocation_return_dbus_error(call, "org.bluez.Error.Failed", "pair failed");
    } else if (g_str_equal(name, "Pair") || g_str_equal(name, "Connect") ||
               g_str_equal(name, "CancelPairing")) {
        held.push_back(G_DBUS_METHOD_INVOCATION(g_object_ref(call)));
    } else if (g_str_equal(name, "StopDiscovery")) {
        g_dbus_method_invocation_return_dbus_error(call, "org.bluez.Error.Failed", "No discovery started");
    } else {
        if (g_str_equal(name, "RemoveDevice")) ++removals;
        g_dbus_method_invocation_return_value(call, nullptr);
    }
}

GVariant *get_property(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                       const gchar *name, GError **, gpointer)
{
    if (g_str_equal(name, "Powered")) return g_variant_new_boolean(powered);
    return g_variant_new_string("11:22:33:44:55:66");
}

gboolean set_property(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                       const gchar *name, GVariant *value, GError **error, gpointer)
{
    assert(g_str_equal(name, "Powered"));
    const bool next = g_variant_get_boolean(value);
    power_requests.push_back(next);
    if (!next && fail_off) {
        g_dbus_error_set_dbus_error(error, "org.bluez.Error.Failed", "power off failed", nullptr);
        return false;
    }
    powered = next;
    return true;
}

template<class Predicate> void until(Predicate ready)
{
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!ready()) {
        assert(std::chrono::steady_clock::now() < deadline);
        while (g_main_context_iteration(nullptr, false)) {}
        std::this_thread::sleep_for(1ms);
    }
}

using Result = std::pair<int, std::string>;
template<class Start> std::future<Result> issue(Start start)
{
    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();
    start([promise](int code, const std::string &message) { promise->set_value({code, message}); });
    return future;
}
Result await(std::future<Result> future)
{
    until([&] { return future.wait_for(0ms) == std::future_status::ready; });
    return future.get();
}
void release_held()
{
    for (auto *call : held) {
        g_dbus_method_invocation_return_dbus_error(call, "org.bluez.Error.Failed", "late failure");
        g_object_unref(call);
    }
    held.clear();
}
}

int main()
{
    GTestDBus *bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", g_test_dbus_get_bus_address(bus), true);
    GError *error = nullptr;
    auto *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    assert(connection && !error);
    auto *name_reply = g_dbus_connection_call_sync(connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", "org.bluez", 0u), nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &error);
    assert(name_reply && !error);
    g_variant_unref(name_reply);
    auto *node = g_dbus_node_info_new_for_xml(
        "<node><interface name='org.freedesktop.DBus.ObjectManager'><method name='GetManagedObjects'>"
        "<arg type='a{oa{sa{sv}}}' direction='out'/></method></interface>"
        "<interface name='org.bluez.Adapter1'><property name='Powered' type='b' access='readwrite'/>"
        "<property name='Address' type='s' access='read'/><method name='StartDiscovery'/>"
        "<method name='StopDiscovery'/><method name='RemoveDevice'><arg type='o' direction='in'/></method></interface>"
        "<interface name='org.bluez.Device1'><method name='Pair'/><method name='Connect'/><method name='CancelPairing'/></interface>"
        "<interface name='org.bluez.AgentManager1'><method name='RegisterAgent'><arg type='o' direction='in'/>"
        "<arg type='s' direction='in'/></method><method name='RequestDefaultAgent'><arg type='o' direction='in'/></method>"
        "<method name='UnregisterAgent'><arg type='o' direction='in'/></method></interface></node>", &error);
    assert(node && !error);
    static const GDBusInterfaceVTable vtable = {method, get_property, set_property, {}};
    const char *paths[] = {"/", "/org/bluez/hci0", "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF", "/org/bluez"};
    std::vector<guint> registrations;
    for (int i = 0; i < 4; ++i) {
        registrations.push_back(g_dbus_connection_register_object(connection, paths[i], node->interfaces[i],
                                                                   &vtable, nullptr, nullptr, &error));
        assert(registrations.back() && !error);
    }
    cp0_bluez_dbus::initialize();
    until([] { return cp0_bluez_dbus::status().address[0] != '\0'; });
    assert(await(issue(cp0_bluez_dbus::stop_discovery_async)).first == 0);

    // Reset must cancel a live Pair and ignore its late failure cleanup.
    auto pair = issue([](auto done) { cp0_bluez_dbus::pair_async("AA:BB:CC:DD:EE:FF", done); });
    until([] { return !held.empty(); });
    assert(await(issue(cp0_bluez_dbus::reset_async)).first == 0);
    assert(await(std::move(pair)).first != 0);
    release_held();
    assert((power_requests == std::vector<bool>{false, true}));
    assert(powered && cp0_bluez_dbus::status().powered);
    cp0_bt_device_t device{};
    assert(cp0_bluez_dbus::list(&device, 1, false) == 1);
    assert(device.paired && !device.connected);
    assert(await(issue(cp0_bluez_dbus::start_discovery_async)).first == 0);

    // A cleanup already waiting on CancelPairing must not remove a bond after reset.
    fail_pair = true;
    pair = issue([](auto done) { cp0_bluez_dbus::pair_async("AA:BB:CC:DD:EE:FF", done); });
    assert(await(std::move(pair)).first != 0);
    until([] { return !held.empty(); });
    assert(await(issue(cp0_bluez_dbus::reset_async)).first == 0);
    release_held();

    // A failed power-off must still attempt power-on; another reset can recover.
    fail_off = true;
    assert(await(issue(cp0_bluez_dbus::reset_async)).first != 0);
    assert(power_requests.back() && powered);
    fail_off = false;
    assert(await(issue(cp0_bluez_dbus::reset_async)).first == 0);
    assert(await(issue(cp0_bluez_dbus::start_discovery_async)).first == 0);
    assert(removals == 0);

    // Pump the fake service while shutdown unregisters the agent synchronously.
    auto shutdown = std::async(std::launch::async, cp0_bluez_dbus::shutdown);
    until([&] { return shutdown.wait_for(0ms) == std::future_status::ready; });
    shutdown.get();
    for (guint id : registrations) g_dbus_connection_unregister_object(connection, id);
    g_dbus_node_info_unref(node);
    g_object_unref(connection);
    g_test_dbus_down(bus);
    g_object_unref(bus);
}
