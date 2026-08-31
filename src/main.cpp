#include <windows.h>
#include <filesystem>
#include <random>
#include "AudioEngine.h"
#include "InputHook.h"
#include "BrowserPanel.h"
#include "Config.h"

namespace fs = std::filesystem;

static AudioEngine g_audio;
static AppConfig g_config;
static BrowserPanel g_browser;
static std::mt19937 g_rng{std::random_device{}()};

// Looks for "<vkCode>_down.wav" / "<vkCode>_up.wav" style files in the active
// soundpack folder. Swap this for a real bindings map (Config::bindings) once
// you have a UI for assigning specific sounds per key.
static void OnKeyEvent(int vkCode, bool isDown) {
    std::string suffix = isDown ? "_down.wav" : "_up.wav";
    std::string candidate = g_config.activeSoundpackDir + "/" + std::to_string(vkCode) + suffix;

    if (!fs::exists(candidate)) {
        // fall back to a generic click/release sound shared by all keys
        candidate = g_config.activeSoundpackDir + (isDown ? "/click.wav" : "/release.wav");
        if (!fs::exists(candidate)) return;
    }

    float volume = g_config.soundGain;
    float pitch = 1.0f;

    if (g_config.randomizeVolume) {
        std::uniform_real_distribution<float> dist(-g_config.volumeVariance, g_config.volumeVariance);
        volume += dist(g_rng);
    }
    if (g_config.randomizePitch) {
        std::uniform_real_distribution<float> dist(-g_config.pitchVariance, g_config.pitchVariance);
        pitch += dist(g_rng);
    }

    g_audio.TriggerSound(candidate, volume, pitch);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Browser panel occupies the whole window in this minimal shell;
            // carve out a control strip at the top for device/soundpack pickers
            // once you add real UI (Win32 controls, Dear ImGui overlay, etc).
            g_browser.Resize(rc.right - rc.left, rc.bottom - rc.top);
            return 0;
        }
        case WM_DESTROY:
            InputHook::Instance().Uninstall();
            g_audio.Stop();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_config = AppConfig::LoadDefault();
    g_config.LoadFromFile("config.ini"); // ignored if it doesn't exist yet
    fs::create_directories(g_config.activeSoundpackDir);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SoundModMainWindow";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"SoundModMainWindow", L"Sound Mod",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 1280, 800, nullptr, nullptr, hInstance, nullptr);

    // 1) Start audio routing: mic -> virtual cable, with click/release sounds mixed in.
    g_audio.Start(g_config.micDeviceName, g_config.outputDeviceName,
                  g_config.micGain, /*masterVolume=*/1.0f);
    g_audio.SetPassthroughEnabled(g_config.passthroughMic);

    // 2) Install the global key/mouse hook that triggers sounds.
    InputHook::Instance().Install(OnKeyEvent);

    // 3) Bring up the embedded browser to db.ruikasa.lol for
    //    browsing/searching/previewing/downloading soundpacks in-app.
    g_browser.SetDownloadFolder(L"soundpacks");
    g_browser.SetDownloadCompleteCallback([](const std::wstring& path) {
        // A new soundpack file just finished downloading into soundpacks/.
        // Hook this up to auto-extract/refresh the soundpack list in the UI.
    });
    g_browser.Initialize(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_config.SaveToFile("config.ini");
    return 0;
}
