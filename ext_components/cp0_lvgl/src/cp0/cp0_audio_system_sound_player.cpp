#include "cp0_audio_system_sound_player.hpp"

#include "hal_lvgl_bsp.h"
#include "miniaudio.h"

#include <algorithm>
#include <array>
#include <mutex>

class Cp0SystemSoundPlayer::Impl
{
public:
    static constexpr std::size_t kSoundCount = 3;

    Impl()
        : names{"Ding2.wav", "key_back.wav", "key_back.wav"}
    {
        load();
    }

    ~Impl()
    {
        std::lock_guard<std::mutex> lock(mutex);
        unload();
        if (initialized) {
            ma_engine_uninit(&engine);
            ma_context_uninit(&context);
        }
    }

    int initialize()
    {
        if (initialized)
            return 0;
        ma_backend backends[] = {ma_backend_pulseaudio};
        if (ma_context_init(backends, 1, nullptr, &context) != MA_SUCCESS)
            return -1;

        ma_engine_config config = ma_engine_config_init();
        config.pContext = &context;
        config.channels = 2;
        config.sampleRate = 48000;
        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            ma_context_uninit(&context);
            return -2;
        }
        initialized = true;
        return 0;
    }

    int load()
    {
        if (loaded)
            return 0;
        int result = initialize();
        if (result != 0)
            return result;

        for (std::size_t i = 0; i < names.size(); ++i) {
            const std::string path = resolve_asset(names[i]);
            if (path.empty())
                continue;
            if (ma_sound_init_from_file(&engine, path.c_str(), MA_SOUND_FLAG_DECODE,
                                        nullptr, nullptr, &sounds[i]) == MA_SUCCESS)
                slots[i] = true;
        }
        loaded = std::any_of(slots.begin(), slots.end(), [](bool slot) { return slot; });
        return loaded ? 0 : -3;
    }

    void unload()
    {
        for (std::size_t i = 0; i < sounds.size(); ++i) {
            if (slots[i]) {
                ma_sound_uninit(&sounds[i]);
                slots[i] = false;
            }
        }
        loaded = false;
    }

    static std::string resolve_asset(const std::string &name)
    {
        if (name.empty())
            return {};
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
            return name;
        const std::string path = cp0_file_path(name);
        return path.empty() ? name : path;
    }

    ma_context context{};
    ma_engine engine{};
    std::array<ma_sound, kSoundCount> sounds{};
    std::array<bool, kSoundCount> slots{};
    std::array<std::string, kSoundCount> names;
    bool initialized = false;
    bool loaded = false;
    bool enabled = true;
    mutable std::mutex mutex;
};

Cp0SystemSoundPlayer::Cp0SystemSoundPlayer()
    : impl_(std::make_unique<Impl>())
{
}

Cp0SystemSoundPlayer::~Cp0SystemSoundPlayer() = default;

int Cp0SystemSoundPlayer::reload(const std::vector<std::string> &names)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (std::size_t i = 0; i < impl_->names.size() && i < names.size(); ++i)
        if (!names[i].empty())
            impl_->names[i] = names[i];
    impl_->unload();
    return impl_->load();
}

bool Cp0SystemSoundPlayer::play_index(std::size_t index)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->enabled || (impl_->load() != 0 && !impl_->loaded) ||
        index >= impl_->sounds.size() || !impl_->slots[index])
        return false;
    ma_sound *sound = &impl_->sounds[index];
    if (ma_sound_is_playing(sound))
        return true;
    return ma_sound_seek_to_pcm_frame(sound, 0) == MA_SUCCESS &&
           ma_sound_start(sound) == MA_SUCCESS;
}

bool Cp0SystemSoundPlayer::play_named(const std::string &name)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->enabled || (impl_->load() != 0 && !impl_->loaded))
        return false;
    for (std::size_t i = 0; i < impl_->names.size(); ++i) {
        if (impl_->names[i] != name || !impl_->slots[i])
            continue;
        ma_sound *sound = &impl_->sounds[i];
        if (ma_sound_is_playing(sound))
            return true;
        return ma_sound_seek_to_pcm_frame(sound, 0) == MA_SUCCESS &&
               ma_sound_start(sound) == MA_SUCCESS;
    }
    return false;
}

bool Cp0SystemSoundPlayer::contains(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return std::find(impl_->names.begin(), impl_->names.end(), name) != impl_->names.end();
}

void Cp0SystemSoundPlayer::set_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->enabled = enabled;
}

bool Cp0SystemSoundPlayer::enabled() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->enabled;
}

std::size_t Cp0SystemSoundPlayer::sound_count() const
{
    return Impl::kSoundCount;
}
