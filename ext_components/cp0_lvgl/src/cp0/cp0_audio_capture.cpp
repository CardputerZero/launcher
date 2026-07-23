#include "cp0_audio_capture.hpp"
#include "../cp0_callback_contract.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

#include "miniaudio.h"

class Cp0AudioCapture::Impl
{
public:
    static constexpr const char* kTmpFile = "/tmp/rec.tmp.wav";
    static constexpr int kWaveformSize = 128;

    ~Impl() { stop(false); }

    static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count)
    {
        auto* self = static_cast<Impl*>(device->pUserData);
        if(self) self->on_data(input, frame_count);
        (void)output;
    }

    int start()
    {
        if(encoder_)
        {
            report(-4, "working");
            return 0;
        }

        encoder_ = std::make_unique<ma_encoder>();
        device_ = std::make_unique<ma_device>();
        ma_encoder_config encoder_config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, 48000);
        if(ma_encoder_init_file(kTmpFile, &encoder_config, encoder_.get()) != MA_SUCCESS)
        {
            report(-1, "Failed to initialize output file.\n");
            encoder_.reset();
            device_.reset();
            return -1;
        }

        ma_device_config device_config = ma_device_config_init(ma_device_type_capture);
        device_config.capture.format = encoder_->config.format;
        device_config.capture.channels = encoder_->config.channels;
        device_config.sampleRate = encoder_->config.sampleRate;
        device_config.dataCallback = data_callback;
        device_config.pUserData = this;
        if(ma_device_init(nullptr, &device_config, device_.get()) != MA_SUCCESS)
        {
            report(-3, "Failed to initialize capture device.\n");
            ma_encoder_uninit(encoder_.get());
            encoder_.reset();
            device_.reset();
            return -2;
        }
        if(ma_device_start(device_.get()) != MA_SUCCESS)
        {
            ma_device_uninit(device_.get());
            ma_encoder_uninit(encoder_.get());
            encoder_.reset();
            device_.reset();
            report(-3, "Failed to start device.\n");
            return -3;
        }
        return 0;
    }

    void stop(bool report_inactive = true)
    {
        if(!device_)
        {
            if(report_inactive) report(-5, "stop");
            return;
        }
        ma_device_uninit(device_.get());
        if(encoder_) ma_encoder_uninit(encoder_.get());
        device_.reset();
        encoder_.reset();
    }

    int pause() { return device_ && ma_device_stop(device_.get()) == MA_SUCCESS ? 0 : (device_ ? -2 : -1); }
    int resume() { return device_ && ma_device_start(device_.get()) == MA_SUCCESS ? 0 : (device_ ? -2 : -1); }

    int save(const std::string& path)
    {
        if(path.empty()) return -1;
        if(device_) stop();
        if(std::rename(kTmpFile, path.c_str()) == 0) return 0;

        int saved_errno = errno;
        int result = copy_file(kTmpFile, path);
        if(result == 0)
        {
            std::remove(kTmpFile);
            return 0;
        }
        errno = saved_errno;
        return result;
    }

    void on_data(const void* input, ma_uint32 frame_count)
    {
        if(encoder_) ma_encoder_write_pcm_frames(encoder_.get(), input, frame_count, nullptr);
        if(!waveform_enabled_.load() || !input || frame_count == 0) return;
        auto callback = status_callback();
        cp0::callback::invoke(callback, 1, build_waveform(input, frame_count));
    }

    std::string build_waveform(const void* input, ma_uint32 frame_count)
    {
        const auto* samples = static_cast<const int16_t*>(input);
        ma_uint32 channels = encoder_ && encoder_->config.channels > 0 ? encoder_->config.channels : 1;
        ma_uint32 sample_count = frame_count * channels;
        int16_t peak = 0;
        double sum_sq = 0.0;
        for(ma_uint32 i = 0; i < sample_count; ++i)
        {
            if(std::abs(samples[i]) > std::abs(peak)) peak = samples[i];
            double sample = static_cast<double>(samples[i]) / 32768.0;
            sum_sq += sample * sample;
        }
        float rms = sample_count ? static_cast<float>(std::sqrt(sum_sq / sample_count)) : 0.0f;
        float db = std::max(-36.0f, 20.0f * std::log10(rms + 1e-6f));
        float normalized = (db + 36.0f) / 36.0f;
        if(peak < 0) normalized = -normalized;

        std::array<float, kWaveformSize> ordered{};
        {
            std::lock_guard<std::mutex> lock(waveform_mutex_);
            waveform_[waveform_index_] = std::max(-1.0f, std::min(1.0f, normalized));
            waveform_index_ = (waveform_index_ + 1) % kWaveformSize;
            for(int i = 0; i < kWaveformSize; ++i) ordered[i] = waveform_[(waveform_index_ + i) % kWaveformSize];
        }
        std::string result(sizeof(float) * kWaveformSize, '\0');
        std::memcpy(&result[0], ordered.data(), result.size());
        return result;
    }

    static int copy_file(const std::string& source_path, const std::string& destination_path)
    {
        FILE* source = std::fopen(source_path.c_str(), "rb");
        if(!source) return -1;
        FILE* destination = std::fopen(destination_path.c_str(), "wb");
        if(!destination)
        {
            std::fclose(source);
            return -2;
        }
        char buffer[4096];
        size_t count = 0;
        int result = 0;
        while((count = std::fread(buffer, 1, sizeof(buffer), source)) > 0)
        {
            if(std::fwrite(buffer, 1, count, destination) != count)
            {
                result = -3;
                break;
            }
        }
        if(std::ferror(source)) result = -4;
        std::fclose(destination);
        std::fclose(source);
        return result;
    }

    StatusCallback status_callback()
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        return callback_;
    }

    void set_status_callback(StatusCallback callback)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    void report(int code, const std::string& message)
    {
        auto callback = status_callback();
        cp0::callback::invoke(callback, code, message);
    }

    StatusCallback callback_;
    std::mutex callback_mutex_;
    std::unique_ptr<ma_device> device_;
    std::unique_ptr<ma_encoder> encoder_;
    std::array<float, kWaveformSize> waveform_{};
    size_t waveform_index_ = 0;
    std::mutex waveform_mutex_;
    std::atomic<bool> waveform_enabled_{false};
};

Cp0AudioCapture::Cp0AudioCapture() : impl_(std::make_unique<Impl>()) {}
Cp0AudioCapture::~Cp0AudioCapture() = default;
void Cp0AudioCapture::set_status_callback(StatusCallback callback) { impl_->set_status_callback(std::move(callback)); }
void Cp0AudioCapture::set_waveform_enabled(bool enabled) { impl_->waveform_enabled_.store(enabled); }
int Cp0AudioCapture::start() { return impl_->start(); }
void Cp0AudioCapture::stop() { impl_->stop(); }
int Cp0AudioCapture::pause() { return impl_->pause(); }
int Cp0AudioCapture::resume() { return impl_->resume(); }
int Cp0AudioCapture::save(const std::string& path) { return impl_->save(path); }
bool Cp0AudioCapture::active() const { return static_cast<bool>(impl_->device_); }
