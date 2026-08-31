#pragma once
#include <functional>
#include <windows.h>

// Wraps a low-level Windows keyboard + mouse hook so the app can play a sound
// the instant a bound key/mouse button goes down or up, system-wide (works
// while the game/other app is focused, same as zcb3's hooking approach).
class InputHook {
public:
    using KeyEventCallback = std::function<void(int virtualKeyCode, bool isDown)>;

    static InputHook& Instance();

    bool Install(KeyEventCallback callback);
    void Uninstall();

private:
    InputHook() = default;
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK m_keyboardHook = nullptr;
    HHOOK m_mouseHook = nullptr;
    KeyEventCallback m_callback;
};
