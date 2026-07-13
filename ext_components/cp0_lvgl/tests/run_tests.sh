#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="${TMPDIR:-/tmp}/cp0_lvgl_test_async_utils.$$"
trap 'rm -f "$binary"' EXIT HUP INT TERM

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -I"$root/include" "$root/tests/test_async_testable_utils.cpp" -o "$binary"
"$binary"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -I"$root/include" "$root/tests/test_dispatch_testable.cpp" -o "$binary"
"$binary"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -I"$root/include" "$root/tests/test_battery_testable.cpp" -o "$binary"
"$binary"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$root/include" -I"$root/src" "$root/src/cp0_sudo_coordinator.cpp" \
    "$root/tests/test_sudo_coordinator.cpp" -o "$binary"
"$binary"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$root/include" -I"$root/src" "$root/src/cp0_process_runner.cpp" \
    "$root/tests/test_process_runner.cpp" -o "$binary"
"$binary"
