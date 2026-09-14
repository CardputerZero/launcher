#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="${TMPDIR:-/tmp}/cp0_bluetooth_reset_test"
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$root/include" \
    "$root/tests/test_bluetooth_reset.cpp" \
    "$root/src/cp0/cp0_bluez_dbus_client.cpp" \
    "$root/src/cp0/cp0_bluetooth_error_policy.cpp" \
    "$root/src/cp0/cp0_lvgl_log.cpp" \
    $(pkg-config --cflags --libs gio-2.0) -o "$binary"
timeout 40s "$binary"
