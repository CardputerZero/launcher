#!/bin/sh
set -eu
python3 "$(dirname "$0")/test_store_cache_sync.py"
build_dir="${TMPDIR:-/tmp}/applaunch-tests"
mkdir -p "$build_dir"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    "$(dirname "$0")/test_low_battery_flow.cpp" \
    -o "$build_dir/test_low_battery_flow"
"$build_dir/test_low_battery_flow"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    "$(dirname "$0")/test_rtc_ntp_state.cpp" \
    "$(dirname "$0")/../main/ui/page_app/setting/rtc_ntp_state.cpp" \
    -o "$build_dir/test_rtc_ntp_state"
"$build_dir/test_rtc_ntp_state"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    -I"$(dirname "$0")/../../../ext_components/cp0_lvgl/include" \
    "$(dirname "$0")/test_adb_state.cpp" \
    "$(dirname "$0")/../main/ui/page_app/setting/adb_state.cpp" \
    -o "$build_dir/test_adb_state"
"$build_dir/test_adb_state"
"$(dirname "$0")/test_cardputer_adb.sh"
python3 "$(dirname "$0")/test_adb_packaging.py"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    -I"$(dirname "$0")/../main/include" \
    -I"$(dirname "$0")/../../../ext_components/cp0_lvgl/include" \
    -I"$(dirname "$0")/../../../SDK/github_source/eventpp/include" \
    "$(dirname "$0")/test_launcher_platform.cpp" \
    -o "$build_dir/test_launcher_platform"
"$build_dir/test_launcher_platform"
