#!/bin/bash
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

set -u
cd "$(dirname "$0")"

SOCK="$PWD/testbus.sock"
CFG="$PWD/dbus.conf"

cleanup() {
	[ -n "${MOCK_PID:-}" ] && kill "$MOCK_PID" 2>/dev/null
	[ -n "${DAEMON_PID:-}" ] && kill "$DAEMON_PID" 2>/dev/null
	rm -f "$SOCK" func_test
}
trap cleanup EXIT

if [ ! -f func_test ]; then
	echo "== build func_test =="
	gcc -std=gnu99 -g -Wall -Wextra -Wno-unused-parameter \
		-DCONFIG_BLUECTL_AGENT_ENABLED=1 -DCONFIG_BLUECTL_MEDIA_ENABLED=1 \
		-I../src $(pkg-config --cflags dbus-1) \
		func_test.c \
		../src/bluectl_core.c \
		../src/bluectl_adapter.c \
		../src/bluectl_device.c \
		../src/bluectl_agent.c \
		../src/bluectl_media.c \
		-o func_test -lpthread $(pkg-config --libs dbus-1) || exit 1
fi

echo "== start private dbus-daemon =="
sed "s|@SOCKET@|$SOCK|g" dbus.conf.in > "$CFG"
dbus-daemon --config-file="$CFG" --print-address > bus_addr.txt &
DAEMON_PID=$!
for i in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.1; done
[ -S "$SOCK" ] || { echo "dbus-daemon failed"; cat bus_addr.txt; exit 1; }
echo "up: $(cat bus_addr.txt)"

export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$SOCK"

echo "== start mock org.bluez =="
python3 mock_bluez.py > mock.log 2>&1 &
MOCK_PID=$!
for i in $(seq 1 50); do grep -q ready mock.log 2>/dev/null && break; sleep 0.1; done
grep -q ready mock.log || { echo "mock failed"; cat mock.log; exit 1; }
echo "ready (pid $MOCK_PID)"

echo "== run func_test =="
./func_test
RV=$?

echo "--- mock.log ---"
cat mock.log
exit $RV
