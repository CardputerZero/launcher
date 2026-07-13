#!/bin/sh
set -eu
build_dir="${TMPDIR:-/tmp}/applaunch-tests"
mkdir -p "$build_dir"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    "$(dirname "$0")/test_rtc_ntp_state.cpp" \
    "$(dirname "$0")/../main/ui/page_app/setting/rtc_ntp_state.cpp" \
    -o "$build_dir/test_rtc_ntp_state"
"$build_dir/test_rtc_ntp_state"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror \
    "$(dirname "$0")/test_adb_state.cpp" \
    "$(dirname "$0")/../main/ui/page_app/setting/adb_state.cpp" \
    -o "$build_dir/test_adb_state"
"$build_dir/test_adb_state"
