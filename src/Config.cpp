#include "Config.h"
#include <fstream>
#include <sstream>

AppConfig AppConfig::LoadDefault() {
    AppConfig cfg;
    cfg.micDeviceName = "";               // "" = system default mic
    cfg.outputDeviceName = "CABLE Input (VB-Audio Virtual Cable)";
    cfg.passthroughMic = true;
    cfg.micGain = 1.0f;
    cfg.soundGain = 1.0f;
    cfg.activeSoundpackDir = "soundpacks/default";
    cfg.randomizePitch = true;
    cfg.pitchVariance = 0.05f;
    cfg.randomizeVolume = true;
    cfg.volumeVariance = 0.1f;
    return cfg;
}

// Minimal flat key=value config format (avoids pulling in a JSON dependency
// for the CI build). Swap for nlohmann/json later if you want nested presets.
bool AppConfig::SaveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "micDeviceName=" << micDeviceName << "\n";
    out << "outputDeviceName=" << outputDeviceName << "\n";
    out << "passthroughMic=" << (passthroughMic ? 1 : 0) << "\n";
    out << "micGain=" << micGain << "\n";
    out << "soundGain=" << soundGain << "\n";
    out << "activeSoundpackDir=" << activeSoundpackDir << "\n";
    out << "randomizePitch=" << (randomizePitch ? 1 : 0) << "\n";
    out << "pitchVariance=" << pitchVariance << "\n";
    out << "randomizeVolume=" << (randomizeVolume ? 1 : 0) << "\n";
    out << "volumeVariance=" << volumeVariance << "\n";
    return true;
}

bool AppConfig::LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "micDeviceName") micDeviceName = val;
        else if (key == "outputDeviceName") outputDeviceName = val;
        else if (key == "passthroughMic") passthroughMic = (val == "1");
        else if (key == "micGain") micGain = std::stof(val);
        else if (key == "soundGain") soundGain = std::stof(val);
        else if (key == "activeSoundpackDir") activeSoundpackDir = val;
        else if (key == "randomizePitch") randomizePitch = (val == "1");
        else if (key == "pitchVariance") pitchVariance = std::stof(val);
        else if (key == "randomizeVolume") randomizeVolume = (val == "1");
        else if (key == "volumeVariance") volumeVariance = std::stof(val);
    }
    return true;
}
