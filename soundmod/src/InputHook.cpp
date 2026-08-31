#include "InputHook.h"

InputHook& InputHook::Instance() {
    static InputHook instance;
    return instance;
}

LRESULT CALLBACK InputHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        if ((isDown || isUp) && Instance().m_callback) {
            Instance().m_callback(kb->vkCode, isDown);
        }
    }
    return CallNextHookEx(Instance().m_keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK InputHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && Instance().m_callback) {
        switch (wParam) {
            case WM_LBUTTONDOWN: Instance().m_callback(VK_LBUTTON, true); break;
            case WM_LBUTTONUP:   Instance().m_callback(VK_LBUTTON, false); break;
            case WM_RBUTTONDOWN: Instance().m_callback(VK_RBUTTON, true); break;
            case WM_RBUTTONUP:   Instance().m_callback(VK_RBUTTON, false); break;
        }
    }
    return CallNextHookEx(Instance().m_mouseHook, nCode, wParam, lParam);
}

bool InputHook::Install(KeyEventCallback callback) {
    m_callback = std::move(callback);
    HINSTANCE hInstance = GetModuleHandle(NULL);

    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

    return m_keyboardHook != nullptr && m_mouseHook != nullptr;
}

void InputHook::Uninstall() {
    if (m_keyboardHook) { UnhookWindowsHookEx(m_keyboardHook); m_keyboardHook = nullptr; }
    if (m_mouseHook) { UnhookWindowsHookEx(m_mouseHook); m_mouseHook = nullptr; }
}
