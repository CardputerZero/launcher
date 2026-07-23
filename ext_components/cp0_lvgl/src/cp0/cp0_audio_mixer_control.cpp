#include "cp0_audio_mixer_control.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>

bool Cp0AudioMixerControl::parse_volume_line(const char *line, int &percent)
{
    if (!line)
        return false;
    const char *marker = std::strchr(line, '%');
    if (!marker)
        return false;
    const char *start = marker;
    while (start > line && start[-1] >= '0' && start[-1] <= '9')
        --start;
    if (start == marker)
        return false;
    if (start > line && !std::isspace(static_cast<unsigned char>(start[-1])))
        return false;

    int parsed = 0;
    const auto result = std::from_chars(start, marker, parsed);
    if (result.ec != std::errc{} || result.ptr != marker || parsed < 0 || parsed > 100)
        return false;
    percent = parsed;
    return true;
}

int Cp0AudioMixerControl::read_volume() const
{
    FILE *pipe = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null", "r");
    if (!pipe)
        return -1;

    char buffer[256];
    int volume = -1;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (parse_volume_line(buffer, volume))
            break;
    }
    pclose(pipe);
    return volume;
}

int Cp0AudioMixerControl::write_volume(int percent) const
{
    percent = clamp_percent(percent);
    char command[128];
    std::snprintf(command, sizeof(command),
                  "pactl set-sink-volume @DEFAULT_SINK@ %d%%", percent);
    return std::system(command) == 0 ? percent : -1;
}

int Cp0AudioMixerControl::read_mute() const
{
    FILE *pipe = popen("pactl get-sink-mute @DEFAULT_SINK@ 2>/dev/null", "r");
    if (!pipe)
        return -1;

    char buffer[128] = {};
    int muted = -1;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (std::strstr(buffer, "yes")) {
            muted = 1;
            break;
        }
        if (std::strstr(buffer, "no")) {
            muted = 0;
            break;
        }
    }
    pclose(pipe);
    return muted;
}

int Cp0AudioMixerControl::toggle_mute() const
{
    if (std::system("pactl set-sink-mute @DEFAULT_SINK@ toggle >/dev/null 2>&1") != 0)
        return -1;
    return read_mute();
}

int Cp0AudioMixerControl::clamp_percent(int percent)
{
    return std::max(0, std::min(100, percent));
}
