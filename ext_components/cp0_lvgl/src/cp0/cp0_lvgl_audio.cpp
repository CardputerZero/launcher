#include "hal_lvgl_bsp.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

extern "C" __attribute__((weak)) const char *hal_path_audio_dir(void)
{
    return nullptr;
}

namespace {

#ifdef _WIN32
constexpr const char *kAudioPathSep = "\\";
#else
constexpr const char *kAudioPathSep = "/";
#endif

std::string audioAssetPath(const std::string &name)
{
    const char *dir = hal_path_audio_dir();
    if (dir == nullptr || name.empty()) {
        return {};
    }

    std::string path(dir);
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path += kAudioPathSep;
    }
    path += name;
    return path;
}

} // namespace

class AudioSystem
{
public:
    AudioSystem()
    {
        initialize();
    }

    int initialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            return 0;
        }

        ma_backend backends[] = {
            ma_backend_pulseaudio
        };

        ma_result result = ma_context_init(
            backends,
            sizeof(backends) / sizeof(backends[0]),
            nullptr,
            &context_
        );

        if (result != MA_SUCCESS) {
            std::fprintf(stderr, "[AUDIO] ma_context_init PulseAudio failed: %d\n", result);
            return -1;
        }

        ma_engine_config engineConfig = ma_engine_config_init();
        engineConfig.pContext = &context_;
        engineConfig.pPlaybackDeviceID = nullptr;
        engineConfig.channels = 2;
        engineConfig.sampleRate = 48000;

        result = ma_engine_init(&engineConfig, &engine_);
        if (result != MA_SUCCESS) {
            std::fprintf(stderr, "[AUDIO] ma_engine_init PulseAudio failed: %d\n", result);
            ma_context_uninit(&context_);
            return -1;
        }

        initialized_ = true;
        std::printf("[AUDIO] audio system initialized with PulseAudio backend\n");
        return 0;
    }

    int loadSounds()
    {
        if (initialize() != 0) {
            return -1;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (soundsLoaded_) {
            return 0;
        }

        const std::string switchPath = audioAssetPath("switch.wav");
        const std::string enterPath = audioAssetPath("enter.wav");
        if (switchPath.empty() || enterPath.empty()) {
            std::fprintf(stderr, "[AUDIO] audio path unavailable\n");
            return -1;
        }

        ma_result result = ma_sound_init_from_file(
            &engine_,
            switchPath.c_str(),
            MA_SOUND_FLAG_DECODE,
            nullptr,
            nullptr,
            &switchSound_
        );

        if (result != MA_SUCCESS) {
            std::fprintf(stderr, "[AUDIO] load switch.wav failed: %d, path=%s\n",
                         result,
                         switchPath.c_str());
            return -1;
        }

        result = ma_sound_init_from_file(
            &engine_,
            enterPath.c_str(),
            MA_SOUND_FLAG_DECODE,
            nullptr,
            nullptr,
            &enterSound_
        );

        if (result != MA_SUCCESS) {
            std::fprintf(stderr, "[AUDIO] load enter.wav failed: %d, path=%s\n",
                         result,
                         enterPath.c_str());
            ma_sound_uninit(&switchSound_);
            return -1;
        }

        soundsLoaded_ = true;
        std::printf("[AUDIO] sounds loaded\n");
        return 0;
    }

    void playSwitch()
    {
        playLoadedSound(switchSound_);
    }

    void playEnter()
    {
        playLoadedSound(enterSound_);
    }

    void play(const std::string &wav)
    {
        if (wav == "switch.wav" || wav == "switch") {
            playSwitch();
            return;
        }

        if (wav == "enter.wav" || wav == "enter") {
            playEnter();
            return;
        }

        std::string path = wav;
        if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos) {
            path = audioAssetPath(wav);
        }

        playFile(path);
    }

    void playFile(const std::string &path)
    {
        if (path.empty() || initialize() != 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        ma_result result = ma_engine_play_sound(&engine_, path.c_str(), nullptr);
        if (result != MA_SUCCESS) {
            std::fprintf(stderr, "[AUDIO] play_audio failed: %d, path=%s\n",
                         result,
                         path.c_str());
        }
    }

    void uninitialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (soundsLoaded_) {
            ma_sound_uninit(&switchSound_);
            ma_sound_uninit(&enterSound_);
            soundsLoaded_ = false;
        }

        if (initialized_) {
            ma_engine_uninit(&engine_);
            ma_context_uninit(&context_);
            initialized_ = false;
        }
    }
    ~AudioSystem()
    {
        uninitialize();
    }
private:
    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;

    void playLoadedSound(ma_sound &sound)
    {
        if (loadSounds() != 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        ma_sound_stop(&sound);
        ma_sound_seek_to_pcm_frame(&sound, 0);
        ma_sound_start(&sound);
    }

    std::mutex mutex_;
    ma_context context_{};
    ma_engine engine_{};
    ma_sound switchSound_{};
    ma_sound enterSound_{};
    bool initialized_ = false;
    bool soundsLoaded_ = false;
};

extern "C" void init_audio(void)
{
    std::shared_ptr<AudioSystem> audio = std::make_shared<AudioSystem>();
    cp0_signal_audio.connect([audio](std::string wav) {
        audio->play(wav);
    });
}
