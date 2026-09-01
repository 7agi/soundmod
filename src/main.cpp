#include <windows.h>
#include <combaseapi.h>
#include <filesystem>
#include <random>
#include <memory>
#include "AudioEngine.h"
#include "InputHook.h"
#include "BrowserPanel.h"
#include "Config.h"

namespace fs = std::filesystem;

// These must NOT be plain file-scope globals: their constructors would run
// during C++ static initialization, before wWinMain gets a chance to call
// CoInitializeEx(COINIT_APARTMENTTHREADED). In particular, AudioEngine's
// constructor calls miniaudio's ma_context_init, and miniaudio's WASAPI
// backend calls CoInitializeEx(COINIT_MULTITHREADED) internally on first use
// - which locks the thread's COM apartment mode before our own call runs,
// causing WebView2's environment creation to fail with RPC_E_CHANGED_MODE
// (0x80010106). Constructing them explicitly, after CoInitializeEx, avoids
// the race entirely regardless of C++ static-init order.
static std::unique_ptr<AudioEngine> g_audio;
static AppConfig g_config;
static std::unique_ptr<BrowserPanel> g_browser;
static std::mt19937 g_rng{std::random_device{}()};

// Looks for "<vkCode>_down.wav" / "<vkCode>_up.wav" style files in the active
// soundpack folder. Swap this for a real bindings map (Config::bindings) once
// you have a UI for assigning specific sounds per key.
static void OnKeyEvent(int vkCode, bool isDown) {
    if (!g_audio) return;

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

    g_audio->TriggerSound(candidate, volume, pitch);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            if (!g_browser) return 0; // WM_SIZE can fire during CreateWindowExW, before g_browser exists
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Browser panel occupies the whole window in this minimal shell;
            // carve out a control strip at the top for device/soundpack pickers
            // once you add real UI (Win32 controls, Dear ImGui overlay, etc).
            g_browser->Resize(rc.right - rc.left, rc.bottom - rc.top);
            return 0;
        }
        case WM_DESTROY:
            InputHook::Instance().Uninstall();
            if (g_audio) g_audio->Stop();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // WebView2 (and miniaudio's WASAPI backend) both need COM initialized on
    // this thread, and WebView2 specifically requires apartment-threaded mode.
    // This MUST be the first thing that could possibly touch COM - see the
    // comment above g_audio/g_browser for why they're constructed below,
    // not as plain globals.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

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

    g_audio = std::make_unique<AudioEngine>();
    g_browser = std::make_unique<BrowserPanel>();

    // 1) Start audio routing: mic -> virtual cable, with click/release sounds mixed in.
    g_audio->Start(g_config.micDeviceName, g_config.outputDeviceName,
                   g_config.micGain, /*masterVolume=*/1.0f);
    g_audio->SetPassthroughEnabled(g_config.passthroughMic);

    // 2) Install the global key/mouse hook that triggers sounds.
    InputHook::Instance().Install(OnKeyEvent);

    // 3) Bring up the embedded browser to db.ruikasa.lol for
    //    browsing/searching/previewing/downloading soundpacks in-app.
    g_browser->SetDownloadFolder(L"soundpacks");
    g_browser->SetDownloadCompleteCallback([](const std::wstring& path) {
        // A new soundpack file just finished downloading into soundpacks/.
        // Hook this up to auto-extract/refresh the soundpack list in the UI.
    });
    g_browser->Initialize(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_config.SaveToFile("config.ini");
    g_browser.reset();
    g_audio.reset();
    CoUninitialize();
    return 0;
}
