#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Run: projects/APPLaunch/tests/low_battery_ui/run_tests.sh [-j N]

set -euo pipefail

jobs=${JOBS:-8}
while (($#)); do
    case "$1" in
        -j)
            jobs=${2:?Missing job count after -j}
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [-j N] (default: JOBS=8; compilers: CC and CXX)"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done
if [[ ! $jobs =~ ^[1-9][0-9]*$ ]]; then
    echo "Job count must be a positive integer" >&2
    exit 2
fi

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$test_dir/../../../.." && pwd)
lvgl="$root/SDK/github_source/lvgl/lvgl_9_5/lvgl"
component="$root/ext_components/cp0_lvgl"
project="$root/projects/APPLaunch"
sdl_cflags_text=$(pkg-config --cflags sdl2)
sdl_libs_text=$(pkg-config --libs sdl2)
read -r -a sdl_cflags <<< "$sdl_cflags_text"
read -r -a sdl_libs <<< "$sdl_libs_text"
read -r -a cc <<< "${CC:-gcc}"
read -r -a cxx <<< "${CXX:-g++}"

build_dir=$(mktemp -d "${TMPDIR:-/tmp}/applaunch-battery-ui-tests.XXXXXX")
pids=()
wait_for_compilers() {
    local status=0 pid
    for pid in "${pids[@]}"; do
        wait "$pid" || status=1
    done
    pids=()
    return "$status"
}
cleanup() {
    wait_for_compilers || true
    rm -rf -- "$build_dir"
}
trap cleanup EXIT

common_flags=(
    -DLV_CONF_INCLUDE_SIMPLE -DLV_KCONFIG_IGNORE
    "-DTEST_LOCKSCREEN_BACKGROUND=\"A:$project/APPLaunch/share/images/lofoten_320x150.png\""
    "-I$test_dir" "-I$lvgl/.." "-I$lvgl" "-I$component/include"
    "-I$project/main/include" "-I$root/SDK/github_source/eventpp/include"
    "-I$root/SDK/components/utilities/include"
)

compile() {
    local source=$1 object=$2
    shift 2
    local -a compiler=("${cc[@]}") language_flags=()
    if [[ $source == *.cpp ]]; then
        compiler=("${cxx[@]}")
        language_flags=(-std=c++17)
    fi
    mkdir -p -- "$(dirname -- "$object")"
    "${compiler[@]}" "${language_flags[@]}" "${common_flags[@]}" \
        "${sdl_cflags[@]}" "$@" -c "$source" -o "$object" &
    pids+=("$!")
    if ((${#pids[@]} >= jobs)); then
        wait_for_compilers
    fi
}

objects=()
while IFS= read -r -d '' source; do
    object="$build_dir/lvgl/${source#"$lvgl/"}.o"
    objects+=("$object")
    compile "$source" "$object"
done < <(find "$lvgl/src" -type f -name '*.c' ! -name lv_sdl_keyboard.c -print0)

for name in cp0_keyboard_queue cp0_keyboard_text cp0_esc_state; do
    object="$build_dir/cp0/$name.o"
    objects+=("$object")
    compile "$component/src/$name.c" "$object"
done
for name in model/screensaver_model.cpp screensaver_fallback.c \
            launcher_media_controls.cpp model/launcher_media_model.cpp \
            model/setup_value_policy.cpp; do
    object="$build_dir/ui/$name.o"
    objects+=("$object")
    compile "$project/main/ui/$name" "$object"
done

for backend in device sdl; do
    backend_flags=()
    if [[ $backend == sdl ]]; then
        backend_flags=(-DTEST_SDL_BACKEND)
    fi
    for name in keyboard_backend.c test_low_battery_ui.cpp; do
        compile "$test_dir/$name" "$build_dir/$backend/$name.o" "${backend_flags[@]}"
    done
done
wait_for_compilers

for backend in device sdl; do
    "${cxx[@]}" "$build_dir/$backend/keyboard_backend.c.o" \
        "$build_dir/$backend/test_low_battery_ui.cpp.o" "${objects[@]}" \
        -lm -pthread "${sdl_libs[@]}" -o "$build_dir/test_$backend"
    echo "Running $backend keyboard backend tests"
    "$build_dir/test_$backend"
done
