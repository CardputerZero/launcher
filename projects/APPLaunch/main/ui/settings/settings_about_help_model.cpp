/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_about_help_model.hpp"

#include <cstddef>

namespace settings_t12b::about_help {
namespace {

std::string value_or_unknown(std::string_view value)
{
    return value.empty() ? "unknown" : std::string(value);
}


std::vector<std::string> split_lines(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t end = text.find('\n', begin);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(begin));
            break;
        }
        lines.emplace_back(text.substr(begin, end - begin));
        begin = end + 1;
    }
    return lines;
}

} // namespace

Content about(std::string_view version,
              std::string_view build_date,
              std::string_view channel,
              std::string_view commit)
{
    return {
        "About",
        {
            "M5CardputerZero",
            "LVGL: 9.x",
            "Version: " + value_or_unknown(version),
            "Build: " + value_or_unknown(build_date),
            "Channel: " + value_or_unknown(channel),
            "Commit: " + value_or_unknown(commit),
        },
    };
}

Content credit()
{
    return {
        "Third-Party Licenses & Credits",
        split_lines(R"CP0_LICENSES(Our sincere thanks to all Kickstarter backers who helped make CardputerZero possible.

CardputerZero is built with open-source software, fonts and media, along with attributed public-domain and Creative Commons resources.

- Software:

Debian 13 trixie
https://www.debian.org

Raspberry Pi OS
https://www.raspberrypi.com/software

Linux kernel
https://github.com/raspberrypi/linux
GPL-2.0-only WITH Linux-syscall-note

Raspberry Pi firmware
https://github.com/raspberrypi/firmware
Mixed licenses

M5Stack Linux driver overlays
https://github.com/m5stack/m5stack-linux-dtoverlays
GPL-2.0 / BSD-2-Clause, file-dependent

M5Stack Linux Libs
https://github.com/m5stack/M5Stack_Linux_Libs
MIT

LVGL 9.5
https://github.com/lvgl/lvgl/tree/v9.5.0
MIT

FreeType
https://gitlab.freedesktop.org/freetype/freetype
FTL OR GPL-2.0-only

libinput
https://gitlab.freedesktop.org/libinput/libinput
MIT

libxkbcommon
https://github.com/xkbcommon/libxkbcommon
MIT

libudev
https://github.com/systemd/systemd
LGPL-2.1-or-later

D-Bus
https://gitlab.freedesktop.org/dbus/dbus
AFL-2.1 OR GPL-2.0-or-later

BlueZ
https://github.com/bluez/bluez
GPL-2.0-or-later / LGPL-2.1-or-later

NetworkManager
https://gitlab.freedesktop.org/NetworkManager/NetworkManager
GPL-2.0-or-later

GLib
https://gitlab.gnome.org/GNOME/glib
LGPL-2.1-or-later

PipeWire
https://gitlab.freedesktop.org/pipewire/pipewire
MIT

WirePlumber
https://gitlab.freedesktop.org/pipewire/wireplumber
MIT

PulseAudio
https://gitlab.freedesktop.org/pulseaudio/pulseaudio
LGPL-2.1-or-later / GPL-2.0-or-later

miniaudio 0.11.25
https://github.com/mackron/miniaudio/tree/0.11.25
Unlicense OR MIT-0

RadioLib
https://github.com/jgromes/RadioLib
MIT

Pigweed
https://pigweed.dev
Apache-2.0

Fuchsia stdcompat
https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/sdk/lib/stdcompat
BSD-3-Clause

eventpp
https://github.com/wqking/eventpp
Apache-2.0

sigslot
https://github.com/palacaze/sigslot
MIT

libhv
https://github.com/ithewei/libhv
BSD-3-Clause

OpenSSL
https://github.com/openssl/openssl
Apache-2.0

TinyALSA
https://github.com/tinyalsa/tinyalsa
BSD-3-Clause

spdlog 1.17
https://github.com/gabime/spdlog/tree/v1.17.0
MIT

fmt
https://github.com/fmtlib/fmt
MIT

Smooth UI Toolkit 2.13
https://github.com/Forairaaaaa/smooth_ui_toolkit/tree/v2.13.0
MIT

M5Unit-NFC
https://github.com/m5stack/M5Unit-NFC
MIT

minmea
https://github.com/kosma/minmea
WTFPL-2.0 OR MIT OR LGPL-3.0-or-later

Box2D 3.1.1
https://github.com/erincatto/box2d/tree/v3.1.1
MIT

libjpeg-turbo 3.0.4
https://github.com/libjpeg-turbo/libjpeg-turbo/tree/3.0.4
IJG + BSD-3-Clause + Zlib

TagLib 2.0.2
https://github.com/taglib/taglib/tree/v2.0.2
LGPL-2.1-or-later OR MPL-1.1

SQLiteCpp 3.3.3
https://github.com/SRombauts/SQLiteCpp/tree/3.3.3
MIT

SQLite
https://github.com/sqlite/sqlite
Public Domain

zlib
https://github.com/madler/zlib
Zlib

libpng
https://github.com/pnggroup/libpng
libpng-2.0

libcamera and IPA modules
https://github.com/raspberrypi/libcamera
LGPL-2.1-or-later / GPL-2.0-or-later

cJSON
https://github.com/DaveGamble/cJSON
MIT

Video4Linux utilities and libv4l
https://github.com/gjasny/v4l-utils
LGPL-2.1-or-later and other per-file licenses

ZeroMQ
https://github.com/zeromq/libzmq
MPL-2.0

iperf3
https://github.com/esnet/iperf
BSD-3-Clause

lksctp-tools
https://github.com/sctp/lksctp-tools
GPL-2.0-or-later

LIRC
https://github.com/aldebaran/lirc
GPL-2.0-only

libgpiod
https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git
LGPL-2.1-or-later

libserialport
https://sigrok.org/wiki/Libserialport
LGPL-3.0-or-later

libyaml
https://github.com/yaml/libyaml
MIT

FFmpeg
https://ffmpeg.org
LGPL-2.1-or-later / GPL-2.0-or-later, build-dependent

nlohmann/json
https://github.com/nlohmann/json
MIT

Proxmark3 crapto1 and mfkey tools
https://github.com/RfidResearchGroup/proxmark3
GPL-3.0-or-later

LodePNG
https://github.com/lvandeve/lodepng
Zlib

ThorVG
https://github.com/thorvg/thorvg
MIT

AnimatedGIF
https://github.com/bitbank2/AnimatedGIF
Apache-2.0

TJpgDec
http://elm-chan.org/fsw/tjpgd/00index.html
BSD-style license

printf
https://github.com/mpaland/printf
MIT

tree.hh 3.20
http://github.com/kpeeters/tree.hh
GPL-3.0-only

TinySoundFont
https://github.com/schellingb/TinySoundFont
MIT

- Resources:

Alibaba PuHuiTi 3.0
https://www.alibabafonts.com
Alibaba PuHuiTi Font License Agreement

Liberation Mono
https://github.com/liberationfonts/liberation-fonts
SIL OFL-1.1

Montserrat
https://github.com/JulietaUla/Montserrat
SIL OFL-1.1

DejaVu Sans
https://github.com/dejavu-fonts/dejavu-fonts
Bitstream Vera and Arev licenses

Source Han Sans
https://github.com/adobe-fonts/source-han-sans
SIL OFL-1.1

Unscii
http://viznut.fi/unscii
Public Domain

Inter
https://github.com/rsms/inter
SIL OFL-1.1

Kenney Input Prompts
https://kenney.nl/assets/input-prompts
CC0-1.0

Phosphor Icons
https://github.com/phosphor-icons/core
MIT

Chivo and Chivo Mono
https://github.com/Omnibus-Type/Chivo
SIL OFL-1.1

Noto Sans, Noto Sans CJK and Noto Sans Mono
https://github.com/notofonts
SIL OFL-1.1

JetBrains Mono
https://github.com/JetBrains/JetBrainsMono
SIL OFL-1.1

UI SFX 0.4.0
https://github.com/romainsimon/uisfx/tree/v0.4.0
CC0-1.0

FluidR3 Yamaha Grand Piano SoundFont
https://packages.debian.org/sid/fluid-soundfont-gm
MIT

MuseScore General Lite
https://musescore.org/en/node/269869
MIT; included components are Public Domain or CC0

Splendid Grand piano samples
MuseScore General Lite
Public Domain

Maple Leaf Rag
Scott Joplin; Mutopia MIDI
Public Domain

Clair de Lune
Bernd Krueger MIDI arrangement
CC BY-SA 3.0 DE

Liebesträume No. 3
Bernd Krueger MIDI arrangement
CC BY-SA 3.0 DE

Daisy Bell (Bicycle Built for Two)
Words and music by Harry Dacre; 1961 IBM 7094 performance by John L. Kelly Jr., Carol Lockbaum and Max V. Mathews

- Sample files in Downloads:

Earthrise
NASA; photographed by William Anders during Apollo 8
https://science.nasa.gov/resource/image-earthrise/
NASA Media Usage Guidelines

Apollo 11 Bootprint
NASA; photographed by Edwin “Buzz” Aldrin
https://science.nasa.gov/resource/apollo-11-bootprint/
NASA Media Usage Guidelines

The Horse in Motion
Eadweard Muybridge; animated by Nevit Dilmen
https://commons.wikimedia.org/wiki/File:The_Horse_in_Motion-anim.gif
Public Domain Mark 1.0

Spring from The Four Seasons
Antonio Vivaldi; performed by John Harrison and the Wichita State University Chamber Players, conducted by Robert Turizziani
https://commons.wikimedia.org/wiki/File:Vivaldi_-_Four_Seasons_1_Spring_mvt_1_Allegro_-_John_Harrison_violin.oga
CC BY-SA 4.0

How Linux is Built
The Linux Foundation
https://www.linuxfoundation.org/blog/blog/how-is-linux-built-our-new-report-and-video
CC BY 3.0

Apollo 11 Guidance Computer excerpt: BURN_BABY_BURN
Virtual AGC project and MIT Museum
https://github.com/chrislgarry/Apollo-11/blob/master/Luminary099/BURN_BABY_BURN--MASTER_IGNITION_ROUTINE.agc
Public Domain)CP0_LICENSES"),
    };
}

} // namespace settings_t12b::about_help
