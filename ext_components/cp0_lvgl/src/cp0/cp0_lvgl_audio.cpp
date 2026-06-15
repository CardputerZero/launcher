#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


class AudioSystem
{
public:
    static constexpr const char* kCapTmpFile = "/tmp/rec.tmp.wav";
    typedef std::function<void(int, std::string)> callback_t;
    typedef std::list<std::string> arg_t;

    AudioSystem()
    {
        init_system_sounds();
    }

    ~AudioSystem()
    {
        uninit_system_play();
    }

public:
    std::function<void(int, std::string)> _cap_status_callback;
    std::unique_ptr<ma_device> ma_cp0_cap_device;
    std::unique_ptr<ma_encoder> ma_cp0_cap_encoder;

    std::unique_ptr<ma_device>  ma_cp0_play_device;
    std::unique_ptr<ma_decoder> ma_cp0_play_decoder;
    std::atomic<bool> play_finished_reported_{false};

    ma_context system_play_context_{};
    ma_engine system_play_engine_{};
    std::array<ma_sound, 3> system_sounds_{};
    std::array<bool, 3> system_sound_loaded_slots_{};
    bool system_play_inited_ = false;
    bool system_sounds_loaded_ = false;
    std::mutex system_play_mutex_;
    std::array<std::string, 3> system_sound_names_ = {"startup.mp3", "switch.wav", "enter.wav"};
    bool system_sound_enabled_ = true;

    static constexpr int kRecWaveformSize = 128;
    std::array<float, kRecWaveformSize> rec_waveform_{};
    size_t rec_waveform_index_ = 0;
    std::mutex rec_waveform_mutex_;
    std::atomic<bool> rec_waveform_enabled_{false};

    static void cap_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        AudioSystem* self = (AudioSystem*)pDevice->pUserData;
        if (self) self->on_cap_data(pInput, frameCount);
        (void)pOutput;
    }

    static void play_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        AudioSystem* self = (AudioSystem*)pDevice->pUserData;
        // ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
        if (self == NULL || self->ma_cp0_play_decoder.get() == NULL) {
            ma_silence_pcm_frames(pOutput, frameCount, pDevice->playback.format, pDevice->playback.channels);
            return;
        }
        ma_uint64 framesRead ;
        ma_result result = ma_decoder_read_pcm_frames(self->ma_cp0_play_decoder.get(), pOutput, frameCount, &framesRead);
        if (framesRead < frameCount) {
            void* silence = ma_offset_pcm_frames_ptr(pOutput, framesRead, pDevice->playback.format, pDevice->playback.channels);
            ma_silence_pcm_frames(silence, frameCount - framesRead, pDevice->playback.format, pDevice->playback.channels);
        }
        bool finished = (result == MA_AT_END || framesRead < frameCount);
        if(finished && !self->play_finished_reported_.exchange(true) && self->_cap_status_callback)
            self->_cap_status_callback(0, "play over\n");
        (void)pInput;
    }



    int play(std::string wav)
    {
        ma_result result;
        ma_device_config deviceConfig;
        stop_play_device(false);
        play_finished_reported_.store(false);
        ma_cp0_play_device = std::make_unique<ma_device>();
        ma_cp0_play_decoder = std::make_unique<ma_decoder>();
        result = ma_decoder_init_file(wav.c_str(), NULL, ma_cp0_play_decoder.get());
        if (result != MA_SUCCESS) {
            if(_cap_status_callback)_cap_status_callback(-2, "Could not load file\n");
            ma_cp0_play_decoder.reset();
            ma_cp0_play_device.reset();
            return -2;
        }

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = ma_cp0_play_decoder.get()->outputFormat;
        deviceConfig.playback.channels = ma_cp0_play_decoder.get()->outputChannels;
        deviceConfig.sampleRate        = ma_cp0_play_decoder.get()->outputSampleRate;
        deviceConfig.dataCallback      = play_data_callback;
        deviceConfig.pUserData         = this;

        if (ma_device_init(NULL, &deviceConfig, ma_cp0_play_device.get()) != MA_SUCCESS) {
            ma_decoder_uninit(ma_cp0_play_decoder.get());
            if(_cap_status_callback)_cap_status_callback(-3, "Failed to open playback device.\n");
            ma_cp0_play_decoder.reset();
            ma_cp0_play_device.reset();
            return -3;
        }

        if (ma_device_start(ma_cp0_play_device.get()) != MA_SUCCESS) {
            ma_device_uninit(ma_cp0_play_device.get());
            ma_decoder_uninit(ma_cp0_play_decoder.get());
            if(_cap_status_callback)_cap_status_callback(-4, "Failed to start playback device.\n");
            ma_cp0_play_decoder.reset();
            ma_cp0_play_device.reset();
            return -4;
        }

        return 0;
    }









private:
    void report(callback_t callback, int code, const std::string& data)
    {
        if(callback) callback(code, data);
        else if(_cap_status_callback) _cap_status_callback(code, data);
    }

    static std::string first_arg_after_command(const arg_t& arg)
    {
        if(arg.size() < 2) return "";
        return *std::next(arg.begin());
    }

    static bool has_path_separator(const std::string& path)
    {
        return path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
    }

    static std::string resolve_play_file(const std::string& file, bool asset)
    {
        if(file.empty()) return "";
        if(!asset || has_path_separator(file)) return file;

        std::string path = cp0_file_path(file);
        return path.empty() ? file : path;
    }

    int init_system_play_locked()
    {
        if(system_play_inited_) return 0;

        ma_backend backends[] = {
            ma_backend_pulseaudio
        };

        ma_result result = ma_context_init(
            backends,
            sizeof(backends) / sizeof(backends[0]),
            NULL,
            &system_play_context_
        );
        if(result != MA_SUCCESS) return -1;

        ma_engine_config engineConfig = ma_engine_config_init();
        engineConfig.pContext = &system_play_context_;
        engineConfig.pPlaybackDeviceID = NULL;
        engineConfig.channels = 2;
        engineConfig.sampleRate = 48000;

        result = ma_engine_init(&engineConfig, &system_play_engine_);
        if(result != MA_SUCCESS)
        {
            ma_context_uninit(&system_play_context_);
            return -2;
        }

        system_play_inited_ = true;
        return 0;
    }

    int load_system_sounds_locked()
    {
        if(system_sounds_loaded_) return 0;

        int ret = init_system_play_locked();
        if(ret != 0) return ret;

        for(size_t i = 0; i < system_sound_names_.size(); ++i)
        {
            std::string path = resolve_play_file(system_sound_names_[i], true);
            if(path.empty()) continue;

            ma_result result = ma_sound_init_from_file(
                &system_play_engine_,
                path.c_str(),
                MA_SOUND_FLAG_DECODE,
                NULL,
                NULL,
                &system_sounds_[i]
            );
            if(result != MA_SUCCESS)
            {
                printf("load system sound failed: %s\n", path.c_str());
                continue;
            }
            system_sound_loaded_slots_[i] = true;
        }

        system_sounds_loaded_ = std::any_of(
            system_sound_loaded_slots_.begin(),
            system_sound_loaded_slots_.end(),
            [](bool loaded) { return loaded; }
        );
        if(!system_sounds_loaded_) return -3;
        return 0;
    }

    void unload_system_sounds_locked()
    {
        for(size_t i = 0; i < system_sounds_.size(); ++i)
        {
            if(system_sound_loaded_slots_[i])
            {
                ma_sound_uninit(&system_sounds_[i]);
                system_sound_loaded_slots_[i] = false;
            }
        }
        system_sounds_loaded_ = false;
    }

    int reload_system_sounds_locked()
    {
        unload_system_sounds_locked();
        return load_system_sounds_locked();
    }

    void uninit_system_play()
    {
        std::lock_guard<std::mutex> lock(system_play_mutex_);

        if(system_sounds_loaded_)
        {
            unload_system_sounds_locked();
        }

        if(system_play_inited_)
        {
            ma_engine_uninit(&system_play_engine_);
            ma_context_uninit(&system_play_context_);
            system_play_inited_ = false;
        }
    }

    void stop_play_device(bool report_state)
    {
        play_finished_reported_.store(false);
        if(ma_cp0_play_device)
        {
            ma_device_uninit(ma_cp0_play_device.get());
            ma_cp0_play_device.reset();
        }
        if(ma_cp0_play_decoder)
        {
            ma_decoder_uninit(ma_cp0_play_decoder.get());
            ma_cp0_play_decoder.reset();
        }
        if(report_state && _cap_status_callback) _cap_status_callback(0, "play stop\n");
    }

    static int copy_file(const std::string& src_path, const std::string& dst_path)
    {
        FILE* src = std::fopen(src_path.c_str(), "rb");
        if(!src) return -1;

        FILE* dst = std::fopen(dst_path.c_str(), "wb");
        if(!dst)
        {
            std::fclose(src);
            return -2;
        }

        char buf[4096];
        size_t n = 0;
        int ret = 0;
        while((n = std::fread(buf, 1, sizeof(buf), src)) > 0)
        {
            if(std::fwrite(buf, 1, n, dst) != n)
            {
                ret = -3;
                break;
            }
        }
        if(std::ferror(src)) ret = -4;

        std::fclose(dst);
        std::fclose(src);
        return ret;
    }

    int save_cap_file(const std::string& dst_path)
    {
        if(dst_path.empty()) return -1;
        if(std::rename(kCapTmpFile, dst_path.c_str()) == 0) return 0;

        int saved_errno = errno;
        int ret = copy_file(kCapTmpFile, dst_path);
        if(ret == 0)
        {
            std::remove(kCapTmpFile);
            return 0;
        }
        errno = saved_errno;
        return ret;
    }

    void on_cap_data(const void* input, ma_uint32 frameCount)
    {
        if(ma_cp0_cap_encoder)
        {
            ma_encoder_write_pcm_frames(ma_cp0_cap_encoder.get(), input, frameCount, NULL);
        }

        if(!rec_waveform_enabled_.load() || !_cap_status_callback || input == NULL || frameCount == 0)
        {
            return;
        }

        std::string waveform = build_rec_waveform(input, frameCount);
        if(!waveform.empty())
        {
            _cap_status_callback(1, waveform);
        }
    }

    std::string build_rec_waveform(const void* input, ma_uint32 frameCount)
    {
        const int16_t* samples = static_cast<const int16_t*>(input);
        ma_uint32 channels = 1;
        if(ma_cp0_cap_encoder && ma_cp0_cap_encoder.get()->config.channels > 0)
        {
            channels = ma_cp0_cap_encoder.get()->config.channels;
        }

        ma_uint32 sampleCount = frameCount * channels;
        int16_t peak = 0;
        double sumSq = 0.0;
        for(ma_uint32 i = 0; i < sampleCount; i++)
        {
            if(std::abs(samples[i]) > std::abs(peak))
            {
                peak = samples[i];
            }
            double s = static_cast<double>(samples[i]) / 32768.0;
            sumSq += s * s;
        }

        float rms = (sampleCount > 0) ? static_cast<float>(std::sqrt(sumSq / sampleCount)) : 0.0f;
        float db = 20.0f * std::log10(rms + 1e-6f);
        if(db < -36.0f) db = -36.0f;
        float dbNorm = (db + 36.0f) / 36.0f;
        if(peak < 0) dbNorm = -dbNorm;

        std::array<float, kRecWaveformSize> waveform{};
        {
            std::lock_guard<std::mutex> lock(rec_waveform_mutex_);
            rec_waveform_[rec_waveform_index_] = std::max(-1.0f, std::min(1.0f, dbNorm));
            rec_waveform_index_ = (rec_waveform_index_ + 1) % kRecWaveformSize;

            for(int i = 0; i < kRecWaveformSize; i++)
            {
                size_t idx = (rec_waveform_index_ + kRecWaveformSize - kRecWaveformSize + i) % kRecWaveformSize;
                waveform[i] = rec_waveform_[idx];
            }
        }

        std::string out(sizeof(float) * kRecWaveformSize, '\0');
        std::memcpy(&out[0], waveform.data(), out.size());
        return out;
    }

    static bool arg_is_enable(const std::string& arg)
    {
        return arg == "1" || arg == "on" || arg == "true" || arg == "enable" || arg == "enabled";
    }

    static bool arg_is_disable(const std::string& arg)
    {
        return arg == "0" || arg == "off" || arg == "false" || arg == "disable" || arg == "disabled";
    }

    int start_cap_device()
    {
        ma_result result;
        ma_encoder_config encoderConfig;
        ma_device_config deviceConfig;
        if(!ma_cp0_cap_encoder)
        {
            ma_cp0_cap_encoder = std::make_unique<ma_encoder>();
            ma_cp0_cap_device = std::make_unique<ma_device>();

            encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, 48000);
            if (ma_encoder_init_file(kCapTmpFile, &encoderConfig, ma_cp0_cap_encoder.get()) != MA_SUCCESS) {
                if(_cap_status_callback)_cap_status_callback(-1, "Failed to initialize output file.\n");
                ma_cp0_cap_encoder.reset();
                ma_cp0_cap_device.reset();
                return -1;
            }
            deviceConfig = ma_device_config_init(ma_device_type_capture);
            deviceConfig.capture.format   = ma_cp0_cap_encoder.get()->config.format;
            deviceConfig.capture.channels = ma_cp0_cap_encoder.get()->config.channels;
            deviceConfig.sampleRate       = ma_cp0_cap_encoder.get()->config.sampleRate;
            deviceConfig.dataCallback     = cap_data_callback;
            deviceConfig.pUserData        = this;
            result = ma_device_init(NULL, &deviceConfig, ma_cp0_cap_device.get());
            if (result != MA_SUCCESS) {
                if(_cap_status_callback)_cap_status_callback(-3, "Failed to initialize capture device.\n");
                ma_encoder_uninit(ma_cp0_cap_encoder.get());
                ma_cp0_cap_encoder.reset();
                ma_cp0_cap_device.reset();
                return -2;
            }
            result = ma_device_start(ma_cp0_cap_device.get());
            if (result != MA_SUCCESS) {
                ma_device_uninit(ma_cp0_cap_device.get());
                ma_encoder_uninit(ma_cp0_cap_encoder.get());
                ma_cp0_cap_encoder.reset();
                ma_cp0_cap_device.reset();
                if(_cap_status_callback)_cap_status_callback(-3, "Failed to start device.\n");
                return -3;
            }
        }
        else
        {
            if(_cap_status_callback)_cap_status_callback(-4, "working");
        }
        return 0;
    }
    void stop_cap_device()
    {
        if(ma_cp0_cap_device)
        {
            ma_device_uninit(ma_cp0_cap_device.get());
            if(ma_cp0_cap_encoder) ma_encoder_uninit(ma_cp0_cap_encoder.get());
            ma_cp0_cap_device.reset();
            ma_cp0_cap_encoder.reset();
        }
        else
        {
            if(_cap_status_callback)_cap_status_callback(-5, "stop");
        }
    }
public:
    int init_system_sounds()
    {
        std::lock_guard<std::mutex> lock(system_play_mutex_);
        return load_system_sounds_locked();
    }

    void system_play(std::string name)
    {
        if(!system_sound_enabled_) return;

        {
            std::lock_guard<std::mutex> lock(system_play_mutex_);
            if(!system_sounds_loaded_ && load_system_sounds_locked() != 0) return;

            for(size_t i = 0; i < system_sound_names_.size(); ++i)
            {
                if(system_sound_names_[i] != name) continue;
                if(!system_sound_loaded_slots_[i]) return;

                ma_sound* sound = &system_sounds_[i];
                if(ma_sound_is_playing(sound)) return;
                ma_sound_seek_to_pcm_frame(sound, 0);
                ma_sound_start(sound);
                return;
            }
        }

        std::string file = resolve_play_file(name, true);
        if(!file.empty()) play(file);
    }

    void system_play_index(size_t index)
    {
        if(!system_sound_enabled_) return;

        std::lock_guard<std::mutex> lock(system_play_mutex_);
        if(!system_sounds_loaded_ && load_system_sounds_locked() != 0)
        {
            return;
        }
        if(index >= system_sounds_.size() || !system_sound_loaded_slots_[index])
        {
            return;
        }

        ma_sound* sound = &system_sounds_[index];
        if(ma_sound_is_playing(sound)) return;
        ma_sound_seek_to_pcm_frame(sound, 0);
        ma_sound_start(sound);
    }

    void SetSystemSoundNames(arg_t arg, callback_t callback)
    {
        {
            std::lock_guard<std::mutex> lock(system_play_mutex_);
            auto it = arg.begin();
            if(it != arg.end()) ++it;
            for(size_t i = 0; i < system_sound_names_.size() && it != arg.end(); ++i, ++it)
            {
                if(!it->empty()) system_sound_names_[i] = *it;
            }

            int ret = reload_system_sounds_locked();
            if(ret != 0)
            {
                report(callback, ret, "system sound reload failed\n");
                return;
            }
        }
        report(callback, 0, "ok");
    }

    void SystemSoundPlay(arg_t arg, callback_t callback)
    {
        int index = std::atoi(first_arg_after_command(arg).c_str());
        if(index < 0 || index >= static_cast<int>(system_sound_names_.size()))
        {
            report(callback, -1, "invalid system sound index\n");
            return;
        }
        if(!system_sound_enabled_)
        {
            report(callback, 0, "system sound disabled\n");
            return;
        }
        system_play_index(static_cast<size_t>(index));
        report(callback, 0, "system sound play\n");
    }

    void SystemSoundEnable(arg_t arg, callback_t callback)
    {
        std::string value = first_arg_after_command(arg);
        if(arg_is_disable(value))
            system_sound_enabled_ = false;
        else if(value.empty() || arg_is_enable(value))
            system_sound_enabled_ = true;
        else
            system_sound_enabled_ = std::atoi(value.c_str()) != 0;
        report(callback, 0, system_sound_enabled_ ? "1" : "0");
    }

    void cap(bool enable)
    {
        if(enable)
        {
            start_cap_device();
        }else{
            stop_cap_device();
        }
    }
    void setup(std::list<std::string> arg, std::function<void(int, std::string)> callback)
    {
        if(arg.empty()) return;
        auto arg1 = arg.begin();
        if(*arg1 == "set_callback")
        {
            _cap_status_callback = callback;
        }
        else if(*arg1 == "set_waveform" || *arg1 == "waveform")
        {
            auto arg2 = std::next(arg1);
            if(arg2 != arg.end())
            {
                if(arg_is_enable(*arg2))
                {
                    rec_waveform_enabled_.store(true);
                }
                else if(arg_is_disable(*arg2))
                {
                    rec_waveform_enabled_.store(false);
                }
            }
            else
            {
                rec_waveform_enabled_.store(true);
            }
        }
        else if(*arg1 == "stop_play")
        {
            stop_play_device(false);
        }
    }
    // 录音的过程控制：开始，暂停，恢复播放，结束保存。
    // 播放的过程控制：开始，暂停，恢复播放，播放结束。
    void PlayFile(arg_t arg, callback_t callback)
    {
        std::string file = resolve_play_file(first_arg_after_command(arg), false);
        if(file.empty())
        {
            report(callback, -1, "PlayFile need file\n");
            return;
        }
        int ret = play(file);
        report(callback, ret, ret == 0 ? "play start\n" : "play failed\n");
    }

    void Play(arg_t arg, callback_t callback)
    {
        std::string file = resolve_play_file(first_arg_after_command(arg), true);
        if(file.empty())
        {
            report(callback, -1, "Play need file\n");
            return;
        }
        int ret = play(file);
        report(callback, ret, ret == 0 ? "play start\n" : "play failed\n");
    }

    void PlayPause(arg_t arg, callback_t callback)
    {
        (void)arg;
        if(!ma_cp0_play_device)
        {
            report(callback, -1, "play not started\n");
            return;
        }
        ma_result ret = ma_device_stop(ma_cp0_play_device.get());
        report(callback, ret == MA_SUCCESS ? 0 : -2, ret == MA_SUCCESS ? "play pause\n" : "play pause failed\n");
    }

    void PlayContinue(arg_t arg, callback_t callback)
    {
        (void)arg;
        if(!ma_cp0_play_device)
        {
            report(callback, -1, "play not started\n");
            return;
        }
        ma_result ret = ma_device_start(ma_cp0_play_device.get());
        report(callback, ret == MA_SUCCESS ? 0 : -2, ret == MA_SUCCESS ? "play continue\n" : "play continue failed\n");
    }

    void PlayEnd(arg_t arg, callback_t callback)
    {
        (void)arg;
        stop_play_device(false);
        report(callback, 0, "play stop\n");
    }

    void Cap(arg_t arg, callback_t callback)
    {
        (void)arg;
        int ret = start_cap_device();
        report(callback, ret, ret == 0 ? "cap start\n" : "cap failed\n");
    }

    void CapPause(arg_t arg, callback_t callback)
    {
        (void)arg;
        if(!ma_cp0_cap_device)
        {
            report(callback, -1, "cap not started\n");
            return;
        }
        ma_result ret = ma_device_stop(ma_cp0_cap_device.get());
        report(callback, ret == MA_SUCCESS ? 0 : -2, ret == MA_SUCCESS ? "cap pause\n" : "cap pause failed\n");
    }

    void CapContinue(arg_t arg, callback_t callback)
    {
        (void)arg;
        if(!ma_cp0_cap_device)
        {
            report(callback, -1, "cap not started\n");
            return;
        }
        ma_result ret = ma_device_start(ma_cp0_cap_device.get());
        report(callback, ret == MA_SUCCESS ? 0 : -2, ret == MA_SUCCESS ? "cap continue\n" : "cap continue failed\n");
    }

    void CapEnd(arg_t arg, callback_t callback)
    {
        (void)arg;
        stop_cap_device();
        report(callback, 0, "cap stop\n");
    }

    void CapFileSave(arg_t arg, callback_t callback)
    {
        std::string file = first_arg_after_command(arg);
        if(file.empty())
        {
            report(callback, -1, "CapFileSave need file\n");
            return;
        }
        if(ma_cp0_cap_device)
        {
            stop_cap_device();
        }
        int ret = save_cap_file(file);
        report(callback, ret, ret == 0 ? "cap file saved\n" : "cap file save failed\n");
    }

    void SetCallback(arg_t arg, callback_t callback)
    {
        (void)arg;
        _cap_status_callback = callback;
    }

    void VolumeRead(arg_t arg, callback_t callback)
    {
        (void)arg;
        int val = read_system_volume();
        report(callback, val >= 0 ? 0 : -1, std::to_string(val));
    }

    void VolumeWrite(arg_t arg, callback_t callback)
    {
        int val = parse_volume_arg(arg);
        int ret = write_system_volume(val);
        report(callback, ret >= 0 ? 0 : -1, std::to_string(ret));
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if(arg.empty())
        {
            report(callback, -1, "empty audio api\n");
            return;
        }
#define map_fun(name) {#name, std::bind(&AudioSystem::name, this, std::placeholders::_1, std::placeholders::_2)}

        std::list<std::pair<std::string, std::function<void(arg_t, callback_t)>>> cmd_map = {
            map_fun(PlayFile),
            map_fun(Play),
            map_fun(PlayPause),
            map_fun(PlayContinue),
            map_fun(PlayEnd),
            map_fun(Cap),
            map_fun(CapPause),
            map_fun(CapContinue),
            map_fun(CapEnd),
            map_fun(CapFileSave),
            map_fun(SetCallback),
            map_fun(VolumeRead),
            map_fun(VolumeWrite),
            map_fun(SetSystemSoundNames),
            map_fun(SystemSoundPlay),
            map_fun(SystemSoundEnable)
        };

#undef map_fun

        for (const auto& it : cmd_map)
        {
            if (it.first == arg.front())
            {
                it.second(arg, callback);
                return;
            }
        }
        report(callback, -1, "unknown audio api\n");
    }

    static int read_system_volume()
    {
        FILE *p = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null", "r");
        if (!p) return -1;
        char buf[256];
        int val = -1;
        while (fgets(buf, sizeof(buf), p)) {
            char *pct = strchr(buf, '%');
            if (pct) {
                char *start = pct;
                while (start > buf && start[-1] >= '0' && start[-1] <= '9') {
                    --start;
                }
                val = clamp_percent(atoi(start));
                break;
            }
        }
        pclose(p);
        return val;
    }

    static int write_system_volume(int val)
    {
        val = clamp_percent(val);

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "pactl set-sink-volume @DEFAULT_SINK@ %d%%", val);
        return system(cmd) == 0 ? val : -1;
    }

    static int parse_volume_arg(const arg_t& arg)
    {
        std::string value = first_arg_after_command(arg);
        if (value.empty()) return 0;
        return clamp_percent(std::atoi(value.c_str()));
    }

    static int clamp_percent(int pct)
    {
        return std::max(0, std::min(100, pct));
    }
};

extern "C" void init_audio(void)
{
    std::shared_ptr<AudioSystem> audio = std::make_shared<AudioSystem>();
    cp0_signal_audio_play.append([audio](std::string wav)
                                 { audio->play(wav); });

    cp0_signal_audio_cap.append([audio](bool enable)
                                { audio->cap(enable); });

    cp0_signal_audio_setup.append([audio](std::list<std::string> arg, std::function<void(int, std::string)> callback)
                                  { audio->setup(arg, callback); });

    cp0_signal_audio_api.append([audio](std::list<std::string> arg, std::function<void(int, std::string)> callback)
                                  { audio->api_call(arg, callback); });

    cp0_signal_system_play.append([audio](std::string name)
                                  { audio->system_play(name); });

}
