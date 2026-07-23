#include "../src/cp0_alsa_parser.hpp"

#include <cassert>
#include <string>

int main()
{
    const std::string cards =
        " 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n"
        "                      HDA Intel PCH at 0xf7e10000 irq 31\n"
        " 2 [USB            ]: USB-Audio - Headset\n"
        "invalid line\n";
    assert(cp0_alsa_parser::encode_cards(cards) ==
           "0\tCard 0: HDA Intel PCH\n2\tCard 2: Headset\n");

    const std::string controls =
        "Simple mixer control 'Master',0\n"
        "  Capabilities: pvolume pswitch\n"
        "  Playback channels: Front Left - Front Right\n"
        "  Limits: Playback 0 - 100\n"
        "  Front Left: Playback 67 [67%] [-10.00dB] [on]\n"
        "  Front Right: Playback 67 [67%] [-10.00dB] [on]\n"
        "Simple mixer control 'Input Source',0\n"
        "  Capabilities: enum\n"
        "  Items: 'Mic' 'Line'\n"
        "  Item0: 'Mic'\n";
    assert(cp0_alsa_parser::encode_controls(controls) ==
           "Master\tINTEGER\t0\t100\t1\tFront Left: Playback 67 [67%] [-10.00dB] [on]\t67\n"
           "Input Source\tENUMERATED\t0\t0\t1\tItem0: 'Mic'\t0\n");

    assert(cp0_alsa_parser::encode_controls("garbage\n") == "");

    assert(cp0_alsa_parser::encode_cards(
               "999999999999999999999999 [BAD]: Device - Overflow\n"
               "1junk [BAD]: Device - Junk\n")
               .empty());
    assert(cp0_alsa_parser::encode_controls(
               "Simple mixer control 'Overflow',0\n"
               "  Capabilities: pvolume\n"
               "  Limits: Playback 0 - 999999999999999999999\n"
               "  Mono: Playback 999999999999999999999 [100%]\n") ==
           "Overflow\tINTEGER\t0\t0\t1\tMono: Playback 999999999999999999999 [100%]\t0\n");
}
