#pragma once

class Cp0AudioMixerControl
{
public:
    static bool parse_volume_line(const char *line, int &percent);
    int read_volume() const;
    int write_volume(int percent) const;
    int read_mute() const;
    int toggle_mute() const;

private:
    static int clamp_percent(int percent);
};
