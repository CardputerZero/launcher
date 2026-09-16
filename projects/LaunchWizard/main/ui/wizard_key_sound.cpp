/* SPDX-License-Identifier: MIT */
#include "wizard_key_sound.h"
#include "command_runner.h"

#if __has_include("global_config.h")
#include "global_config.h"
#endif
// The device audio service supplies miniaudio when enabled. SDL's service is a stub.
#if !defined(CONFIG_CP0_LVGL_INIT_AUDIO) || defined(CONFIG_V9_5_LV_USE_SDL)
#define MINIAUDIO_IMPLEMENTATION
#endif
#include "miniaudio.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char **environ;

namespace launch_wizard {

WizardKeySound::~WizardKeySound() { stop(); }

void WizardKeySound::start()
{
    if (child_ > 0) return;
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) return;

    posix_spawn_file_actions_t actions;
    int result = posix_spawn_file_actions_init(&actions);
    if (result == 0) {
        result = posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
        if (result == 0) {
            char executable[] = "/proc/self/exe";
            char option[] = "--key-sound-worker";
            char *args[] = {executable, option, nullptr};
            result = posix_spawn(&child_, executable, &actions, nullptr, args, environ);
        }
        posix_spawn_file_actions_destroy(&actions);
    }
    close(sockets[1]);
    if (result != 0) {
        close(sockets[0]);
        child_ = -1;
        std::fprintf(stderr, "LaunchWizard: cannot start key sound: %s\n", std::strerror(result));
        return;
    }
    socket_ = sockets[0];
}

void WizardKeySound::play(WizardSoundCue cue)
{
    // Audio startup and playback must never block the LVGL input thread.
    if (socket_ >= 0) {
        const char command = static_cast<char>(cue);
        (void)send(socket_, &command, 1, MSG_DONTWAIT | MSG_NOSIGNAL);
    }
}

void WizardKeySound::stop()
{
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    if (child_ <= 0) return;
    // Bound teardown even if the audio server is unavailable or unresponsive.
    for (int attempt = 0; attempt < 100; ++attempt) {
        const pid_t result = waitpid(child_, nullptr, WNOHANG);
        if (result == child_ || (result < 0 && errno == ECHILD)) {
            child_ = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(child_, SIGKILL);
    while (waitpid(child_, nullptr, 0) < 0 && errno == EINTR) {}
    child_ = -1;
}

namespace {

struct CueDefinition {
    WizardSoundCue cue;
    const char *filename;
    float volume;
};

constexpr std::array<CueDefinition, 6> kCues{{
    {WizardSoundCue::Typing, "launch-wizard-key.wav", 1.00f},
    {WizardSoundCue::Lock, "launch-wizard-lock.wav", 0.67f},
    {WizardSoundCue::Unlock, "launch-wizard-unlock.wav", 0.70f},
    {WizardSoundCue::Error, "launch-wizard-error.wav", 0.70f},
    {WizardSoundCue::Confirm, "launch-wizard-notification.wav", 0.432f},
    {WizardSoundCue::Complete, "launch-wizard-achievement.wav", 0.72f},
}};

std::string key_sound_path(const char *filename)
{
    const std::string asset = std::string("share/audio/") + filename;
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        // SCons places resources beside the executable in dist/APPLaunch.
        const auto bundled = executable.parent_path() / "APPLaunch" / asset;
        if (std::filesystem::is_regular_file(bundled, error)) return bundled.string();
        // Installed layout: APPLaunch/bin/LaunchWizard and APPLaunch/share/audio.
        const auto installed = executable.parent_path().parent_path() / asset;
        if (std::filesystem::is_regular_file(installed, error)) return installed.string();
    }
    return std::string("/usr/share/APPLaunch/") + asset;
}

bool prepare_audio_user()
{
    if (geteuid() != 0) return true;
    const passwd *user = getpwuid(1000);
    if (!user) return false;
    const std::string name = user->pw_name;
    const std::string home = user->pw_dir;
    const gid_t gid = user->pw_gid;
    CommandOptions options;
    options.timeout = std::chrono::seconds(3);
    options.terminate_grace = std::chrono::milliseconds(100);
    (void)run_command_process({"systemctl", "start", "--no-block", "user@1000.service"},
                              nullptr, options);
    // Only this exec'ed helper drops privileges; the wizard retains its setup permissions.
    if (initgroups(name.c_str(), gid) != 0 || setgid(gid) != 0 || setuid(1000) != 0)
        return false;
    setenv("HOME", home.c_str(), 1);
    setenv("USER", name.c_str(), 1);
    setenv("LOGNAME", name.c_str(), 1);
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    unsetenv("PULSE_SERVER");
    unsetenv("PULSE_RUNTIME_PATH");
    unsetenv("PULSE_COOKIE");
    for (int attempt = 0; attempt < 15; ++attempt) {
        if (access("/run/user/1000/pulse/native", F_OK) == 0) return true;
        pollfd input{STDIN_FILENO, 0, 0};
        if (poll(&input, 1, 200) > 0) return false;
    }
    return false;
}

} // namespace

int run_key_sound_worker()
{
    if (!prepare_audio_user()) {
        std::fprintf(stderr, "LaunchWizard: key sound user session unavailable\n");
        return 1;
    }
    ma_context context;
    const ma_backend backend = ma_backend_pulseaudio;
    ma_result result = ma_context_init(&backend, 1, nullptr, &context);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "LaunchWizard: key sound context: %s\n", ma_result_description(result));
        return 1;
    }
    ma_engine engine;
    ma_engine_config config = ma_engine_config_init();
    config.pContext = &context;
    config.channels = 2;
    config.sampleRate = 48000;
    result = ma_engine_init(&config, &engine);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "LaunchWizard: key sound engine: %s\n", ma_result_description(result));
        ma_context_uninit(&context);
        return 1;
    }
    ma_engine_set_volume(&engine, 1.5f); // Match Keyboard-Guide's master and per-cue gains.
    std::array<ma_sound, kCues.size()> sounds{};
    std::array<bool, kCues.size()> loaded{};
    bool loaded_any = false;
    for (std::size_t index = 0; index < kCues.size(); ++index) {
        const std::string path = key_sound_path(kCues[index].filename);
        result = ma_sound_init_from_file(&engine, path.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr, nullptr, &sounds[index]);
        if (result == MA_SUCCESS) {
            loaded[index] = true;
            loaded_any = true;
            ma_sound_set_volume(&sounds[index], kCues[index].volume);
        } else {
            std::fprintf(stderr, "LaunchWizard: key sound asset '%s': %s\n",
                         path.c_str(), ma_result_description(result));
        }
    }
    if (loaded_any) {
        // Warm the stream before starting short feedback sounds.
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        char cues[128];
        ma_sound *active = nullptr;
        bool completing = false;
        for (;;) {
            const ssize_t count = read(STDIN_FILENO, cues, sizeof(cues));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            for (ssize_t offset = 0; offset < count; ++offset) {
                // Let the success cue finish before accepting more key feedback.
                if (completing && active && ma_sound_is_playing(active)) continue;
                for (std::size_t index = 0; index < kCues.size(); ++index) {
                    if (cues[offset] != static_cast<char>(kCues[index].cue) || !loaded[index])
                        continue;
                    if (active) ma_sound_stop(active);
                    active = &sounds[index];
                    ma_sound_seek_to_pcm_frame(active, 0);
                    ma_sound_start(active);
                    completing = kCues[index].cue == WizardSoundCue::Complete;
                    break;
                }
            }
        }
        // Preserve the final confirmation click before account migration stops us.
        for (int attempt = 0; attempt < 90 && active && ma_sound_is_playing(active); ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (std::size_t index = 0; index < sounds.size(); ++index)
        if (loaded[index]) ma_sound_uninit(&sounds[index]);
    ma_engine_uninit(&engine);
    ma_context_uninit(&context);
    return loaded_any ? 0 : 1;
}

} // namespace launch_wizard
