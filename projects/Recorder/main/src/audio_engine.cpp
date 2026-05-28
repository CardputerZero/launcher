#include "audio_engine.h"
#include "wav_file.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cstring>

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
    uint8_t playChannels = 1;
    uint32_t playTotalFrames = 0;
    uint32_t playDataOffset = 0;
    uint32_t playCurrentFrame = 0;

    std::atomic<AudioState> state{AudioState::Idle};
    std::atomic<bool> playbackFinished{false};
    AudioEngine::UpdateCallback updateCb;

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
    impl_->playTotalFrames = dataSize / (channels * sizeof(int16_t));
    impl_->playCurrentFrame = 0;
    impl_->playDataOffset = ftell(impl_->playFile);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = impl_->playChannels;
    config.sampleRate = impl_->playSampleRate;
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
    impl_->playbackFinished.store(false);
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
    return static_cast<float>(impl_->playCurrentFrame) / static_cast<float>(impl_->playSampleRate);
}

float AudioEngine::playbackDuration() const
{
    return static_cast<float>(impl_->playTotalFrames) / static_cast<float>(impl_->playSampleRate);
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
    size_t read = fread(output, 1, bytesToRead, impl_->playFile);

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
