#pragma once

#include <functional>
#include <memory>
#include <string>

class Cp0AudioCapture
{
public:
    using StatusCallback = std::function<void(int, std::string)>;

    Cp0AudioCapture();
    ~Cp0AudioCapture();

    Cp0AudioCapture(const Cp0AudioCapture&) = delete;
    Cp0AudioCapture& operator=(const Cp0AudioCapture&) = delete;

    void set_status_callback(StatusCallback callback);
    void set_waveform_enabled(bool enabled);

    int start();
    void stop();
    int pause();
    int resume();
    int save(const std::string& path);
    bool active() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
