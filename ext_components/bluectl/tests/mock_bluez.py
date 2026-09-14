#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

import os
import sys
import threading
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop, threads_init
from gi.repository import GLib

BUS_NAME = "org.bluez"
ADAPTER_PATH = "/org/bluez/hci0"
DEV1_PATH = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"
DEV2_PATH = "/org/bluez/hci0/dev_11_22_33_44_55_66"
PLAYER_PATH = DEV2_PATH + "/player0"

IFACE_ADAPTER = "org.bluez.Adapter1"
IFACE_DEVICE = "org.bluez.Device1"
IFACE_PLAYER = "org.bluez.MediaPlayer1"
IFACE_AGENT_MANAGER = "org.bluez.AgentManager1"
IFACE_OM = "org.freedesktop.DBus.ObjectManager"
IFACE_PROPS = "org.freedesktop.DBus.Properties"

state = {
    "powered": False,
    "discoverable": False,
    "discovering": False,
    "pairable": True,
    "alias": "test-host",
    "discoverable_timeout": 180,
    "pairable_timeout": 0,
    "dev1_trusted": False,
    "dev1_paired": False,
    "registered_agent": None,
    "default_agent": None,
    "agent_capability": None,
    "agent_owner": None,
    "agent_request_count": 0,
    "pair_confirm_reply": None,
    "remove_device": None,
    "media_cmd": None,
}


def adapter_props():
    return {
        "Address": dbus.String("00:11:22:33:44:55"),
        "Name": dbus.String("test-host"),
        "Alias": dbus.String(state["alias"]),
        "Powered": dbus.Boolean(state["powered"]),
        "Discoverable": dbus.Boolean(state["discoverable"]),
        "Discovering": dbus.Boolean(state["discovering"]),
        "Pairable": dbus.Boolean(state["pairable"]),
        "DiscoverableTimeout": dbus.UInt32(state["discoverable_timeout"]),
        "PairableTimeout": dbus.UInt32(state["pairable_timeout"]),
    }


def dev1_props():
    return {
        "Address": dbus.String("AA:BB:CC:DD:EE:FF"),
        "Name": dbus.String("Test Phone"),
        "Alias": dbus.String("Test Phone"),
        "Icon": dbus.String("phone"),
        "Paired": dbus.Boolean(state["dev1_paired"]),
        "Connected": dbus.Boolean(False),
        "Trusted": dbus.Boolean(state["dev1_trusted"]),
        "Blocked": dbus.Boolean(False),
        "RSSI": dbus.Int16(-52),
        "Class": dbus.UInt32(0x260304),
        "UUIDs": dbus.Array(
            [dbus.String("0000110b-0000-1000-8000-00805f9b34fb")],
            signature="s",
        ),
    }


def dev2_props():
    return {
        "Address": dbus.String("11:22:33:44:55:66"),
        "Name": dbus.String("Speaker"),
        "Alias": dbus.String("Speaker"),
        "Paired": dbus.Boolean(True),
        "Connected": dbus.Boolean(True),
        "Trusted": dbus.Boolean(True),
        "Blocked": dbus.Boolean(False),
        "UUIDs": dbus.Array(
            [
                dbus.String("0000110b-0000-1000-8000-00805f9b34fb"),
                dbus.String("0000110e-0000-1000-8000-00805f9b34fb"),
            ],
            signature="s",
        ),
    }


def player_props():
    return {
        "Status": dbus.String("playing"),
        "Position": dbus.UInt32(42000),
        "Track": dbus.Dictionary(
            {
                "Title": dbus.String("Test Song"),
                "Artist": dbus.String("Tester"),
                "Album": dbus.String("Demo"),
                "Genre": dbus.String("Pop"),
                "Duration": dbus.UInt32(210000),
            },
            signature="sv",
        ),
    }


def managed_objects():
    return dbus.Dictionary(
        {
            ADAPTER_PATH: dbus.Dictionary(
                {IFACE_ADAPTER: adapter_props()}, signature="sa{sv}"
            ),
            DEV1_PATH: dbus.Dictionary(
                {IFACE_DEVICE: dev1_props()}, signature="sa{sv}"
            ),
            DEV2_PATH: dbus.Dictionary(
                {IFACE_DEVICE: dev2_props()}, signature="sa{sv}"
            ),
            PLAYER_PATH: dbus.Dictionary(
                {IFACE_PLAYER: player_props()}, signature="sa{sv}"
            ),
        },
        signature="oa{sa{sv}}",
    )


class BluezObject(dbus.service.Object):
    def __init__(self, bus, path):
        self._bus = bus
        super().__init__(bus, path)

    # ---- ObjectManager ----
    @dbus.service.method(IFACE_OM, out_signature="a{oa{sa{sv}}}")
    def GetManagedObjects(self):
        return managed_objects()

    @dbus.service.signal(IFACE_OM, signature="oa{sa{sv}}")
    def InterfacesAdded(self, path, interfaces):
        pass

    @dbus.service.signal(IFACE_OM, signature="oas")
    def InterfacesRemoved(self, path, interfaces):
        pass

    # ---- Properties ----
    @dbus.service.method(IFACE_PROPS, in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        if interface == IFACE_ADAPTER:
            return adapter_props()
        if interface == IFACE_DEVICE:
            if self._object_path == DEV1_PATH:
                return dev1_props()
            return dev2_props()
        if interface == IFACE_PLAYER:
            return player_props()
        raise dbus.exceptions.DBusException(
            "org.freedesktop.DBus.Error.UnknownInterface: " + interface
        )

    @dbus.service.method(IFACE_PROPS, in_signature="ssv", out_signature="")
    def Set(self, interface, name, value):
        if self._object_path == ADAPTER_PATH:
            key = {
                "Powered": "powered",
                "Discoverable": "discoverable",
                "Pairable": "pairable",
                "Alias": "alias",
                "DiscoverableTimeout": "discoverable_timeout",
                "PairableTimeout": "pairable_timeout",
            }.get(name)
            if key is not None:
                state[key] = value
                self.PropertiesChanged(interface, {name: value}, [])
                return
        if self._object_path == DEV1_PATH:
            if name in ("Trusted", "Blocked"):
                state["dev1_" + name.lower()] = value
                self.PropertiesChanged(interface, {name: value}, [])
                return
            if name == "Alias":
                state["dev1_alias"] = str(value)
                self.PropertiesChanged(interface, {name: value}, [])
                return
        if self._object_path == DEV2_PATH and name == "Alias":
            state["dev2_alias"] = str(value)
            self.PropertiesChanged(interface, {name: value}, [])
            return
        raise dbus.exceptions.DBusException(
            "org.freedesktop.DBus.Error.InvalidArgs: no such property " + name
        )

    @dbus.service.signal(IFACE_PROPS, signature="sa{sv}as")
    def PropertiesChanged(self, interface, changed, invalidated):
        pass

    # ---- Adapter1 ----
    @dbus.service.method(IFACE_ADAPTER, in_signature="", out_signature="")
    def StartDiscovery(self):
        state["discovering"] = True
        self.PropertiesChanged(
            IFACE_ADAPTER, {"Discovering": dbus.Boolean(True)}, []
        )

    @dbus.service.method(IFACE_ADAPTER, in_signature="", out_signature="")
    def StopDiscovery(self):
        if not state["discovering"]:
            raise dbus.exceptions.DBusException(
                "org.bluez.Error.NotStarted: No discovery started"
            )
        state["discovering"] = False
        self.PropertiesChanged(
            IFACE_ADAPTER, {"Discovering": dbus.Boolean(False)}, []
        )

    @dbus.service.method(IFACE_ADAPTER, in_signature="o", out_signature="")
    def RemoveDevice(self, device):
        state["remove_device"] = str(device)
        self.InterfacesRemoved(str(device), [IFACE_DEVICE])

    # ---- Device1 ----
    def _request_confirmation(self, owner, reply_cb, error_cb):

        try:
            proxy = self._bus.get_object(owner, state["registered_agent"],
                                          introspect=False)
            agent = dbus.Interface(proxy, "org.bluez.Agent1")
            reply = agent.RequestConfirmation(
                dbus.ObjectPath(DEV1_PATH), dbus.UInt32(123456)
            )
            print("mock: agent reply=%r" % (reply,), flush=True)

            def complete_pair():
                state["pair_confirm_reply"] = reply
                state["dev1_paired"] = True
                self.PropertiesChanged(
                    IFACE_DEVICE, {"Paired": dbus.Boolean(True)}, []
                )
                reply_cb()
                return False

            GLib.idle_add(complete_pair)
        except Exception as exc:
            print("mock: agent request failed: %s" % (exc,), flush=True)
            def fail_pair():
                error_cb(exc)
                return False

            GLib.idle_add(fail_pair)

    @dbus.service.method(IFACE_DEVICE, in_signature="", out_signature="",
                         async_callbacks=("reply_cb", "error_cb"))
    def Pair(self, reply_cb, error_cb):
        if not state["registered_agent"]:
            raise dbus.exceptions.DBusException(
                "org.bluez.Error.NotRegistered: no agent"
            )
        state["agent_request_count"] += 1
        print("mock: Pair -> call agent RequestConfirmation", flush=True)
        owner = state.get("agent_owner")
        if not owner:
            raise dbus.exceptions.DBusException(
                "org.bluez.Error.NotRegistered: no agent owner"
            )


        threading.Thread(target=self._request_confirmation,
                         args=(owner, reply_cb, error_cb), daemon=True).start()

    @dbus.service.method(IFACE_DEVICE, in_signature="", out_signature="")
    def Connect(self):
        self.PropertiesChanged(IFACE_DEVICE, {"Connected": dbus.Boolean(True)}, [])

    @dbus.service.method(IFACE_DEVICE, in_signature="", out_signature="")
    def Disconnect(self):
        self.PropertiesChanged(
            IFACE_DEVICE, {"Connected": dbus.Boolean(False)}, []
        )

    # ---- MediaPlayer1 ----
    @dbus.service.method(IFACE_PLAYER, in_signature="", out_signature="")
    def Play(self):
        state["media_cmd"] = "Play"
        self.PropertiesChanged(IFACE_PLAYER, {"Status": dbus.String("playing")}, [])

    @dbus.service.method(IFACE_PLAYER, in_signature="", out_signature="")
    def Pause(self):
        state["media_cmd"] = "Pause"
        self.PropertiesChanged(IFACE_PLAYER, {"Status": dbus.String("paused")}, [])

    # ---- AgentManager1 ----
    @dbus.service.method(IFACE_AGENT_MANAGER, in_signature="os", out_signature="",
                         sender_keyword="sender")
    def RegisterAgent(self, path, capability, sender=None):
        state["registered_agent"] = str(path)
        state["agent_capability"] = str(capability)

        state["agent_owner"] = sender

    @dbus.service.method(IFACE_AGENT_MANAGER, in_signature="o", out_signature="")
    def UnregisterAgent(self, path):
        if state["registered_agent"] == str(path):
            state["registered_agent"] = None

    @dbus.service.method(IFACE_AGENT_MANAGER, in_signature="o", out_signature="")
    def RequestDefaultAgent(self, path):
        state["default_agent"] = str(path)


def main():
    threads_init()
    global bus_name_holder
    DBusGMainLoop(set_as_default=True)

    addr = os.environ.get("DBUS_SYSTEM_BUS_ADDRESS", "")
    bus = dbus.bus.BusConnection(addr) if addr else dbus.SystemBus()

    bus_name_holder = dbus.service.BusName(BUS_NAME, bus)

    BluezObject(bus, "/org/bluez")
    BluezObject(bus, ADAPTER_PATH)
    BluezObject(bus, DEV1_PATH)
    BluezObject(bus, DEV2_PATH)
    obj = BluezObject(bus, PLAYER_PATH)

    def emit_device_added():
        obj.InterfacesAdded(DEV1_PATH, {IFACE_DEVICE: dev1_props()})
        return False

    GLib.timeout_add(300, emit_device_added)
    print("mock org.bluez ready", flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    sys.exit(main())
