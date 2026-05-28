#include "audio_engine.h"
#include "wav_file.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

struct AudioEngineImpl {
    ma_context context{};
    bool contextInitialized = false;

    ma_device* recDevice = nullptr;
    ma_device* playDevice = nullptr;

    FILE* recFile = nullptr;
    uint32_t recSampleRate = 48000;
    uint8_t recChannels = 1;
    uint32_t recTotalFrames = 0;

    FILE* playFile = nullptr;
    uint32_t playSampleRate = 48000;
    uint32_t deviceSampleRate = 48000;
    uint8_t playChannels = 1;
    uint32_t playTotalFrames = 0;
    uint32_t playDataOffset = 0;
    uint32_t playCurrentFrame = 0;
    float playbackSpeed = 1.0f;

    std::atomic<AudioState> state{AudioState::Idle};
    std::atomic<bool> playbackFinished{false};
    AudioEngine::UpdateCallback updateCb;

    // Recording waveform ring buffer
    static constexpr int kRecWaveformSize = 128;
    std::array<float, kRecWaveformSize> recWaveform{};
    mutable std::mutex recWaveformMutex;
    size_t recWaveformIndex = 0;

    // Playback waveform
    static constexpr int kPlayWaveformSize = 256;
    std::array<float, kPlayWaveformSize> playWaveform{};
    bool hasPlayWaveform = false;

    ~AudioEngineImpl() {
        if (recDevice) {
            ma_device_stop(recDevice);
            ma_device_uninit(recDevice);
            delete recDevice;
        }
        if (playDevice) {
            ma_device_stop(playDevice);
            ma_device_uninit(playDevice);
            delete playDevice;
        }
        if (contextInitialized) {
            ma_context_uninit(&context);
        }
    }
};

extern "C" {
    static void recordingCallback(ma_device* pDevice, void* pOutput,
                                  const void* pInput, ma_uint32 frameCount)
    {
        (void)pOutput;
        AudioEngine* self = static_cast<AudioEngine*>(pDevice->pUserData);
        self->onRecordingData(pInput, frameCount);
    }

    static void playbackCallback(ma_device* pDevice, void* pOutput,
                                 const void* pInput, ma_uint32 frameCount)
    {
        (void)pInput;
        AudioEngine* self = static_cast<AudioEngine*>(pDevice->pUserData);
        self->onPlaybackData(pOutput, frameCount);
    }
}

AudioEngine::AudioEngine() : impl_(std::make_unique<AudioEngineImpl>()) {}
AudioEngine::~AudioEngine() = default;

bool AudioEngine::initialize()
{
    if (impl_->contextInitialized) return true;
    if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS) {
        return false;
    }
    impl_->contextInitialized = true;
    return true;
}

void AudioEngine::shutdown()
{
    stopRecording();
    stopPlayback();
}

bool AudioEngine::startRecording(const std::string& filepath)
{
    if (impl_->state.load() != AudioState::Idle) return false;
    if (!impl_->contextInitialized && !initialize()) return false;

    stopRecording();

    impl_->recFile = fopen(filepath.c_str(), "wb");
    if (!impl_->recFile) return false;
    impl_->recTotalFrames = 0;
    wav_file_write_header(impl_->recFile, impl_->recChannels, impl_->recSampleRate, 0);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_s16;
    config.capture.channels = impl_->recChannels;
    config.sampleRate = impl_->recSampleRate;
    config.dataCallback = recordingCallback;
    config.pUserData = this;

    impl_->recDevice = new ma_device();
    memset(impl_->recDevice, 0, sizeof(ma_device));
    ma_result result = ma_device_init(&impl_->context, &config, impl_->recDevice);
    if (result != MA_SUCCESS) {
        delete impl_->recDevice;
        impl_->recDevice = nullptr;
        fclose(impl_->recFile);
        impl_->recFile = nullptr;
        return false;
    }

    result = ma_device_start(impl_->recDevice);
    if (result != MA_SUCCESS) {
        ma_device_uninit(impl_->recDevice);
        delete impl_->recDevice;
        impl_->recDevice = nullptr;
        fclose(impl_->recFile);
        impl_->recFile = nullptr;
        return false;
    }

    impl_->state.store(AudioState::Recording);
    return true;
}

void AudioEngine::pauseRecording()
{
    if (impl_->state.load() == AudioState::Recording && impl_->recDevice) {
        ma_device_stop(impl_->recDevice);
        impl_->state.store(AudioState::RecPaused);
    }
}

void AudioEngine::resumeRecording()
{
    if (impl_->state.load() == AudioState::RecPaused && impl_->recDevice) {
        ma_device_start(impl_->recDevice);
        impl_->state.store(AudioState::Recording);
    }
}

void AudioEngine::stopRecording()
{
    AudioState s = impl_->state.load();
    if (s != AudioState::Recording && s != AudioState::RecPaused) return;

    if (impl_->recDevice) {
        ma_device_stop(impl_->recDevice);
        ma_device_uninit(impl_->recDevice);
        delete impl_->recDevice;
        impl_->recDevice = nullptr;
    }

    if (impl_->recFile) {
        wav_file_finalize(impl_->recFile);
        fclose(impl_->recFile);
        impl_->recFile = nullptr;
    }
    impl_->recTotalFrames = 0;
    impl_->state.store(AudioState::Idle);
}

bool AudioEngine::startPlayback(const std::string& filepath)
{
    if (impl_->state.load() != AudioState::Idle) return false;
    if (!impl_->contextInitialized && !initialize()) return false;

    stopPlayback();

    impl_->playFile = fopen(filepath.c_str(), "rb");
    if (!impl_->playFile) return false;

    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t dataSize = 0;
    if (!wav_file_read_header(impl_->playFile, channels, sampleRate, dataSize)) {
        fclose(impl_->playFile);
        impl_->playFile = nullptr;
        return false;
    }

    impl_->playChannels = static_cast<uint8_t>(channels);
    impl_->playSampleRate = sampleRate;
    impl_->deviceSampleRate = sampleRate;
    impl_->playbackSpeed = 1.0f;
    impl_->playTotalFrames = dataSize / (channels * sizeof(int16_t));
    impl_->playCurrentFrame = 0;
    impl_->playDataOffset = ftell(impl_->playFile);

    // Generate playback waveform
    {
        long savedPos = ftell(impl_->playFile);
        size_t bytesPerFrame = impl_->playChannels * sizeof(int16_t);
        uint32_t totalFrames = impl_->playTotalFrames;
        int outCount = AudioEngineImpl::kPlayWaveformSize;
        uint32_t framesPerBin = totalFrames / outCount;
        if (framesPerBin < 1) framesPerBin = 1;

        fseek(impl_->playFile, impl_->playDataOffset, SEEK_SET);
        std::vector<int16_t> buffer(framesPerBin * impl_->playChannels);

        for (int i = 0; i < outCount; i++) {
            uint32_t toRead = framesPerBin;
            long curPos = ftell(impl_->playFile);
            fseek(impl_->playFile, 0, SEEK_END);
            long endPos = ftell(impl_->playFile);
            fseek(impl_->playFile, curPos, SEEK_SET);
            uint32_t remaining = static_cast<uint32_t>((endPos - curPos) / static_cast<long>(bytesPerFrame));
            if (toRead > remaining) toRead = remaining;

            if (toRead > 0) {
                if (toRead > buffer.size() / impl_->playChannels) {
                    buffer.resize(toRead * impl_->playChannels);
                }
                size_t read = fread(buffer.data(), bytesPerFrame, toRead, impl_->playFile);
                int16_t peak = 0;
                for (size_t j = 0; j < read * impl_->playChannels; j++) {
                    if (std::abs(buffer[j]) > std::abs(peak)) {
                        peak = buffer[j];
                    }
                }
                impl_->playWaveform[i] = std::abs(peak) / 32768.0f;
            } else {
                impl_->playWaveform[i] = 0.0f;
            }
        }
        impl_->hasPlayWaveform = true;
        fseek(impl_->playFile, savedPos, SEEK_SET);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = impl_->playChannels;
    config.sampleRate = impl_->deviceSampleRate;
    config.dataCallback = playbackCallback;
    config.pUserData = this;

    impl_->playDevice = new ma_device();
    memset(impl_->playDevice, 0, sizeof(ma_device));
    ma_result result = ma_device_init(&impl_->context, &config, impl_->playDevice);
    if (result != MA_SUCCESS) {
        delete impl_->playDevice;
        impl_->playDevice = nullptr;
        fclose(impl_->playFile);
        impl_->playFile = nullptr;
        return false;
    }

    result = ma_device_start(impl_->playDevice);
    if (result != MA_SUCCESS) {
        ma_device_uninit(impl_->playDevice);
        delete impl_->playDevice;
        impl_->playDevice = nullptr;
        fclose(impl_->playFile);
        impl_->playFile = nullptr;
        return false;
    }

    impl_->state.store(AudioState::Playing);
    return true;
}

void AudioEngine::pausePlayback()
{
    if (impl_->state.load() == AudioState::Playing && impl_->playDevice) {
        ma_device_stop(impl_->playDevice);
        impl_->state.store(AudioState::PlayPaused);
    }
}

void AudioEngine::resumePlayback()
{
    if (impl_->state.load() == AudioState::PlayPaused && impl_->playDevice) {
        ma_device_start(impl_->playDevice);
        impl_->state.store(AudioState::Playing);
    }
}

void AudioEngine::stopPlayback()
{
    AudioState s = impl_->state.load();
    if (s != AudioState::Playing && s != AudioState::PlayPaused) return;

    if (impl_->playDevice) {
        ma_device_stop(impl_->playDevice);
        ma_device_uninit(impl_->playDevice);
        delete impl_->playDevice;
        impl_->playDevice = nullptr;
    }

    if (impl_->playFile) {
        fclose(impl_->playFile);
        impl_->playFile = nullptr;
    }
    impl_->playTotalFrames = 0;
    impl_->playCurrentFrame = 0;
    impl_->playbackSpeed = 1.0f;
    impl_->deviceSampleRate = 48000;
    impl_->playbackFinished.store(false);
    impl_->hasPlayWaveform = false;
    impl_->state.store(AudioState::Idle);
}

AudioState AudioEngine::state() const
{
    return impl_->state.load();
}

void AudioEngine::poll()
{
    if (impl_->playbackFinished.exchange(false)) {
        stopPlayback();
    }
}

float AudioEngine::recordingDuration() const
{
    return static_cast<float>(impl_->recTotalFrames) / static_cast<float>(impl_->recSampleRate);
}

float AudioEngine::playbackPosition() const
{
    if (impl_->deviceSampleRate == 0) return 0;
    return static_cast<float>(impl_->playCurrentFrame) / static_cast<float>(impl_->deviceSampleRate);
}

float AudioEngine::playbackDuration() const
{
    if (impl_->playSampleRate == 0) return 0;
    return static_cast<float>(impl_->playTotalFrames) / static_cast<float>(impl_->playSampleRate);
}

void AudioEngine::seekPlayback(float seconds)
{
    if (!impl_->playFile) return;
    float dur = playbackDuration();
    if (seconds < 0) seconds = 0;
    if (seconds > dur) seconds = dur;

    uint32_t fileFrame = static_cast<uint32_t>(seconds * impl_->playSampleRate);
    if (fileFrame > impl_->playTotalFrames) fileFrame = impl_->playTotalFrames;

    size_t bytesPerFrame = impl_->playChannels * sizeof(int16_t);
    fseek(impl_->playFile, static_cast<long>(impl_->playDataOffset + fileFrame * bytesPerFrame), SEEK_SET);
    impl_->playCurrentFrame = static_cast<uint32_t>(seconds * impl_->deviceSampleRate);
}

void AudioEngine::setPlaybackSpeed(float speed)
{
    if (!impl_->playFile || speed <= 0.0f) return;
    if (speed == impl_->playbackSpeed) return;

    AudioState currentState = impl_->state.load();
    if (currentState != AudioState::Playing && currentState != AudioState::PlayPaused) return;

    uint32_t oldDeviceSampleRate = impl_->deviceSampleRate;
    uint32_t currentFrame = impl_->playCurrentFrame;

    if (impl_->playDevice) {
        ma_device_stop(impl_->playDevice);
        ma_device_uninit(impl_->playDevice);
        delete impl_->playDevice;
        impl_->playDevice = nullptr;
    }

    impl_->playbackSpeed = speed;
    impl_->deviceSampleRate = static_cast<uint32_t>(impl_->playSampleRate * speed);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = impl_->playChannels;
    config.sampleRate = impl_->deviceSampleRate;
    config.dataCallback = playbackCallback;
    config.pUserData = this;

    impl_->playDevice = new ma_device();
    memset(impl_->playDevice, 0, sizeof(ma_device));
    ma_result result = ma_device_init(&impl_->context, &config, impl_->playDevice);
    if (result != MA_SUCCESS) {
        impl_->deviceSampleRate = impl_->playSampleRate;
        impl_->playbackSpeed = 1.0f;
        config.sampleRate = impl_->deviceSampleRate;
        result = ma_device_init(&impl_->context, &config, impl_->playDevice);
        if (result != MA_SUCCESS) {
            delete impl_->playDevice;
            impl_->playDevice = nullptr;
            fclose(impl_->playFile);
            impl_->playFile = nullptr;
            impl_->state.store(AudioState::Idle);
            return;
        }
    }

    float posSec = static_cast<float>(currentFrame) / static_cast<float>(oldDeviceSampleRate);
    uint32_t fileFrame = static_cast<uint32_t>(posSec * impl_->playSampleRate);
    if (fileFrame > impl_->playTotalFrames) fileFrame = impl_->playTotalFrames;

    size_t bytesPerFrame = impl_->playChannels * sizeof(int16_t);
    fseek(impl_->playFile, static_cast<long>(impl_->playDataOffset + fileFrame * bytesPerFrame), SEEK_SET);
    impl_->playCurrentFrame = static_cast<uint32_t>(posSec * impl_->deviceSampleRate);

    if (currentState == AudioState::Playing) {
        ma_device_start(impl_->playDevice);
    } else {
        impl_->state.store(AudioState::PlayPaused);
    }
}

void AudioEngine::getRecordingWaveform(float* out, int count) const
{
    if (count > AudioEngineImpl::kRecWaveformSize) count = AudioEngineImpl::kRecWaveformSize;
    if (count <= 0) return;
    std::lock_guard<std::mutex> lock(impl_->recWaveformMutex);
    for (int i = 0; i < count; i++) {
        size_t idx = (impl_->recWaveformIndex + AudioEngineImpl::kRecWaveformSize - count + i) % AudioEngineImpl::kRecWaveformSize;
        out[i] = impl_->recWaveform[idx];
    }
}

void AudioEngine::getPlaybackWaveform(float* out, int count) const
{
    if (count > AudioEngineImpl::kPlayWaveformSize) count = AudioEngineImpl::kPlayWaveformSize;
    if (count <= 0) return;
    if (!impl_->hasPlayWaveform) {
        for (int i = 0; i < count; i++) out[i] = 0;
        return;
    }
    for (int i = 0; i < count; i++) {
        out[i] = impl_->playWaveform[i];
    }
}

bool AudioEngine::hasPlaybackWaveform() const
{
    return impl_->hasPlayWaveform;
}

void AudioEngine::setUpdateCallback(UpdateCallback cb)
{
    impl_->updateCb = cb;
}

void AudioEngine::onRecordingData(const void* input, ma_uint32 frameCount)
{
    if (!impl_->recFile) return;
    size_t bytesToWrite = frameCount * impl_->recChannels * sizeof(int16_t);
    size_t written = fwrite(input, 1, bytesToWrite, impl_->recFile);
    if (written == bytesToWrite) {
        impl_->recTotalFrames += frameCount;

        const int16_t* samples = static_cast<const int16_t*>(input);
        int16_t peak = 0;
        for (ma_uint32 i = 0; i < frameCount * impl_->recChannels; i++) {
            if (std::abs(samples[i]) > std::abs(peak)) {
                peak = samples[i];
            }
        }
        float normalized = peak / 32768.0f;
        {
            std::lock_guard<std::mutex> lock(impl_->recWaveformMutex);
            impl_->recWaveform[impl_->recWaveformIndex] = normalized;
            impl_->recWaveformIndex = (impl_->recWaveformIndex + 1) % AudioEngineImpl::kRecWaveformSize;
        }

        if (impl_->updateCb) impl_->updateCb();
    }
}

void AudioEngine::onPlaybackData(void* output, ma_uint32 frameCount)
{
    if (!impl_->playFile) {
        memset(output, 0, frameCount * impl_->playChannels * sizeof(int16_t));
        return;
    }
    size_t bytesPerFrame = impl_->playChannels * sizeof(int16_t);
    size_t bytesToRead = frameCount * bytesPerFrame;
    long beforePos = ftell(impl_->playFile);
    size_t read = fread(output, 1, bytesToRead, impl_->playFile);
    long afterPos = ftell(impl_->playFile);

    if (read < bytesToRead) {
        memset(static_cast<uint8_t*>(output) + read, 0, bytesToRead - read);
        impl_->playCurrentFrame += static_cast<uint32_t>(read / bytesPerFrame);
        if (read == 0) {
            impl_->playbackFinished.store(true);
        }
    } else {
        impl_->playCurrentFrame += frameCount;
        if (impl_->updateCb) impl_->updateCb();
    }
}
