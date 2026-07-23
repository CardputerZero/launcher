#include "desktop_entry.h"

#include <cassert>
#include <string>

int main()
{
    const auto entry = parse_desktop_entry(
        "[Other]\nName=Ignored\n[Desktop Entry]\r\n"
        " Name = Demo App \r\nIcon = demo.png\n"
        "Exec=/usr/bin/demo --flag=value\nTerminal=True\nSysplause=0\n");
    assert(entry);
    assert(entry->name == "Demo App");
    assert(entry->icon == "demo.png");
    assert(entry->exec == "/usr/bin/demo --flag=value");
    assert(entry->terminal);
    assert(!entry->sysplause);
    assert(!parse_desktop_entry("[Desktop Entry]\nName=Missing Exec\n"));
    assert(!parse_desktop_entry("[Desktop Entry]\nExec=missing-name\n"));

    const auto defaults = parse_desktop_entry("[Desktop Entry]\nName=A\nExec=a\n");
    assert(defaults && !defaults->terminal && defaults->sysplause);

    assert(desktop_entry_filename_valid("demo.desktop"));
    assert(!desktop_entry_filename_valid(".desktop"));
    assert(!desktop_entry_filename_valid("../demo.desktop"));
    assert(!desktop_entry_filename_valid("sub\\demo.desktop"));
    assert(!desktop_entry_filename_valid("bad\n.desktop"));

    assert(!parse_desktop_entry(
        "[Desktop Entry]\nType=Link\nName=A\nExec=python3\n"));
    assert(!parse_desktop_entry(
        "[Desktop Entry]\nHidden=true\nName=A\nExec=python3\n"));
    assert(!parse_desktop_entry(
        "[Desktop Entry]\nNoDisplay=1\nName=A\nExec=python3\n"));
    std::string nul_exec = "[Desktop Entry]\nName=A\nExec=python3";
    nul_exec.push_back('\0');
    nul_exec += " --unsafe\n";
    assert(!parse_desktop_entry(nul_exec));
    assert(!parse_desktop_entry("[Desktop Entry]\nName=Bad\tName\nExec=python3\n"));
    assert(!parse_desktop_entry(
        "[Desktop Entry]\nName=" +
        std::string(DESKTOP_ENTRY_MAX_NAME_BYTES + 1, 'a') + "\nExec=python3\n"));
}
