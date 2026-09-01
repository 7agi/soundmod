#pragma once
#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"
#include <string>
#include <functional>

// Embeds a Microsoft Edge WebView2 control as a child window, navigated to
// the soundpack site. This is what gives you "download, preview and search
// on the software itself" — it's a real browser surface, not a scraper, so
// the site's own search/preview/download UI just works.
//
// Optional: hook OnDownloadStarting to auto-redirect downloaded files
// straight into your soundpacks/ folder instead of the default Downloads dir.
class BrowserPanel {
public:
    bool Initialize(HWND parentWindow, const std::wstring& startUrl = L"https://db.ruikasa.lol/");
    void Resize(int width, int height);
    void Navigate(const std::wstring& url);
    void GoHome();

    // Fired when the user triggers a download in the embedded browser.
    // resultPath = where the file ended up once complete.
    using DownloadCompleteCallback = std::function<void(const std::wstring& resultPath)>;
    void SetDownloadCompleteCallback(DownloadCompleteCallback cb) { m_downloadCallback = std::move(cb); }

    void SetDownloadFolder(const std::wstring& folder) { m_downloadFolder = folder; }

private:
    void HookDownloadEvents();
    void HookNavigationDiagnostics();

    HWND m_parent = nullptr;
    wil::com_ptr<ICoreWebView2Controller> m_controller;
    wil::com_ptr<ICoreWebView2> m_webview;
    std::wstring m_homeUrl = L"https://db.ruikasa.lol/";
    std::wstring m_downloadFolder = L"soundpacks";
    DownloadCompleteCallback m_downloadCallback;
};
