#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"
#include <cstring>
#include <cstdio>

AudioEngine::AudioEngine() {
    if (ma_context_init(NULL, 0, NULL, &m_context) == MA_SUCCESS) {
        m_contextInitialized = true;
    }
}

AudioEngine::~AudioEngine() {
    Stop();
    if (m_contextInitialized) ma_context_uninit(&m_context);
}

std::vector<AudioEngine::DeviceInfo> AudioEngine::ListPlaybackDevices() {
    std::vector<DeviceInfo> result;
    if (!m_contextInitialized) return result;

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount,
                                &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        return result;
    }

    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        DeviceInfo info;
        info.name = pPlaybackInfos[i].name;
        info.id = pPlaybackInfos[i].id;
        result.push_back(info);
    }
    return result;
}

std::vector<AudioEngine::DeviceInfo> AudioEngine::ListCaptureDevices() {
    std::vector<DeviceInfo> result;
    if (!m_contextInitialized) return result;

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount,
                                &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        return result;
    }

    for (ma_uint32 i = 0; i < captureCount; ++i) {
        DeviceInfo info;
        info.name = pCaptureInfos[i].name;
        info.id = pCaptureInfos[i].id;
        result.push_back(info);
    }
    return result;
}

void AudioEngine::MixInto(float* out, ma_uint32 frameCount, ma_uint32 channels) {
    std::lock_guard<std::mutex> lock(m_voicesMutex);
    for (auto it = m_voices.begin(); it != m_voices.end(); ) {
        auto& voice = *it;
        std::vector<float> tmp(frameCount * channels, 0.0f);
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&voice->decoder, tmp.data(), frameCount, &framesRead);

        for (ma_uint32 i = 0; i < framesRead * channels; ++i) {
            out[i] += tmp[i] * voice->volume;
        }

        if (framesRead < frameCount) {
            ma_decoder_uninit(&voice->decoder);
            it = m_voices.erase(it);
        } else {
            ++it;
        }
    }

    // Simple soft clip to avoid harsh distortion when many sounds overlap.
    for (ma_uint32 i = 0; i < frameCount * channels; ++i) {
        if (out[i] > 1.0f) out[i] = 1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

void AudioEngine::DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioEngine* self = static_cast<AudioEngine*>(pDevice->pUserData);
    float* out = static_cast<float*>(pOutput);
    const float* in = static_cast<const float*>(pInput);
    ma_uint32 channels = pDevice->playback.channels;

    std::memset(out, 0, sizeof(float) * frameCount * channels);

    // 1) Mic passthrough
    if (self->m_passthroughEnabled.load() && in != nullptr) {
        float gain = self->m_micGain.load();
        for (ma_uint32 i = 0; i < frameCount * channels; ++i) {
            out[i] += in[i] * gain;
        }
    }

    // 2) Mix in any active click/release one-shot sounds
    self->MixInto(out, frameCount, channels);

    // 3) Master volume
    float master = self->m_masterVolume.load();
    for (ma_uint32 i = 0; i < frameCount * channels; ++i) {
        out[i] *= master;
        if (out[i] > 1.0f) out[i] = 1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

bool AudioEngine::Start(const std::string& captureDeviceName, const std::string& playbackDeviceName,
                         float micGain, float masterVolume) {
    if (!m_contextInitialized) return false;
    if (m_running.load()) Stop();

    m_micGain.store(micGain);
    m_masterVolume.store(masterVolume);

    ma_device_id* pCaptureId = nullptr;
    ma_device_id* pPlaybackId = nullptr;
    ma_device_id captureId{}, playbackId{};

    auto findId = [&](bool isPlayback, const std::string& name, ma_device_id& outId) -> bool {
        auto devices = isPlayback ? ListPlaybackDevices() : ListCaptureDevices();
        for (auto& d : devices) {
            if (d.name == name) { outId = d.id; return true; }
        }
        return false;
    };

    if (!captureDeviceName.empty() && findId(false, captureDeviceName, captureId)) {
        pCaptureId = &captureId;
    }
    if (!playbackDeviceName.empty() && findId(true, playbackDeviceName, playbackId)) {
        pPlaybackId = &playbackId;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_duplex);
    cfg.capture.pDeviceID = pCaptureId;
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = m_channels;
    cfg.playback.pDeviceID = pPlaybackId;
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = m_channels;
    cfg.sampleRate = m_sampleRate;
    cfg.dataCallback = DataCallback;
    cfg.pUserData = this;

    if (ma_device_init(&m_context, &cfg, &m_device) != MA_SUCCESS) {
        return false;
    }
    m_deviceInitialized = true;

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
        return false;
    }

    m_running.store(true);
    return true;
}

void AudioEngine::Stop() {
    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
    }
    m_running.store(false);

    std::lock_guard<std::mutex> lock(m_voicesMutex);
    for (auto& v : m_voices) ma_decoder_uninit(&v->decoder);
    m_voices.clear();
}

bool AudioEngine::TriggerSound(const std::string& filePath, float volume, float pitchScale) {
    auto voice = std::make_unique<ActiveVoice>();
    ma_decoder_config decCfg = ma_decoder_config_init(ma_format_f32, m_channels, m_sampleRate);

    if (ma_decoder_init_file(filePath.c_str(), &decCfg, &voice->decoder) != MA_SUCCESS) {
        return false;
    }

    // Cheap pitch shift: change the decoder's output sample rate ratio via miniaudio's
    // internal resampler by re-initing with a scaled sample rate target.
    if (pitchScale != 1.0f && pitchScale > 0.0f) {
        ma_decoder_uninit(&voice->decoder);
        ma_decoder_config pitchedCfg = ma_decoder_config_init(
            ma_format_f32, m_channels, (ma_uint32)(m_sampleRate * pitchScale));
        if (ma_decoder_init_file(filePath.c_str(), &pitchedCfg, &voice->decoder) != MA_SUCCESS) {
            return false;
        }
    }

    voice->volume = volume;

    std::lock_guard<std::mutex> lock(m_voicesMutex);
    m_voices.push_back(std::move(voice));
    return true;
}
