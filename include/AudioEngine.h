#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "miniaudio.h"

// A currently-playing one-shot sound (a decoded click/release wav being mixed out)
struct ActiveVoice {
    ma_decoder decoder;
    bool finished = false;
    float volume = 1.0f;
};

// Handles:
//  - listing playback/capture devices (so the user can pick the virtual cable)
//  - full-duplex passthrough: mic input -> mixed -> chosen output device
//  - triggering one-shot click/release sounds into the same output mix
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    struct DeviceInfo {
        std::string name;
        ma_device_id id;
    };

    // Enumerate devices so the UI/browser panel can present a picker.
    std::vector<DeviceInfo> ListPlaybackDevices();
    std::vector<DeviceInfo> ListCaptureDevices();

    // Start routing: captureDevice (your mic) -> playbackDevice (virtual cable input)
    // If playbackDevice is empty, uses the system default output.
    bool Start(const std::string& captureDeviceName, const std::string& playbackDeviceName,
               float micGain, float masterVolume);
    void Stop();

    // Queue a one-shot sound file to be mixed into the output stream right now.
    // pitchScale resamples playback speed (1.0 = normal), volume 0..1+.
    bool TriggerSound(const std::string& filePath, float volume, float pitchScale);

    void SetMicGain(float g) { m_micGain.store(g); }
    void SetMasterVolume(float v) { m_masterVolume.store(v); }
    void SetPassthroughEnabled(bool e) { m_passthroughEnabled.store(e); }

    bool IsRunning() const { return m_running.load(); }

private:
    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void MixInto(float* out, ma_uint32 frameCount, ma_uint32 channels);

    ma_context m_context{};
    ma_device m_device{};       // duplex device: capture = mic, playback = virtual cable / output
    bool m_contextInitialized = false;
    bool m_deviceInitialized = false;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_passthroughEnabled{true};
    std::atomic<float> m_micGain{1.0f};
    std::atomic<float> m_masterVolume{1.0f};

    std::mutex m_voicesMutex;
    std::vector<std::unique_ptr<ActiveVoice>> m_voices;

    ma_uint32 m_sampleRate = 48000;
    ma_uint32 m_channels = 2;
};
