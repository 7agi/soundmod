#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// One clickset entry: e.g. "click1.wav" mapped to mouse-down, "release1.wav" to mouse-up
struct SoundEntry {
    std::string filePath;
    bool isRelease = false; // true = key/mouse UP sound, false = DOWN sound
};

struct KeyBinding {
    int virtualKeyCode = 0;           // Windows VK_* code
    std::vector<SoundEntry> downSounds;
    std::vector<SoundEntry> upSounds;
};

struct AppConfig {
    // Audio devices
    std::string micDeviceName;        // input device (your physical mic)
    std::string outputDeviceName;     // e.g. "CABLE Input (VB-Audio Virtual Cable)"
    bool passthroughMic = true;       // mix mic straight into the virtual cable
    float micGain = 1.0f;
    float soundGain = 1.0f;

    // Soundpack
    std::string activeSoundpackDir;   // folder containing click/release wavs
    std::vector<KeyBinding> bindings;

    // Randomization (like zcb3's pitch/volume variation)
    bool randomizePitch = true;
    float pitchVariance = 0.05f;      // +/- 5%
    bool randomizeVolume = true;
    float volumeVariance = 0.1f;

    static AppConfig LoadDefault();
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
};
