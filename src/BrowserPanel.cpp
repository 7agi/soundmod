#include "BrowserPanel.h"
#include <filesystem>
#include <shlwapi.h>

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

bool BrowserPanel::Initialize(HWND parentWindow, const std::wstring& startUrl) {
    m_parent = parentWindow;
    m_homeUrl = startUrl;

    fs::create_directories(m_downloadFolder);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;

                env->CreateCoreWebView2Controller(
                    m_parent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT res, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(res) || !controller) return res;

                            m_controller = controller;
                            m_controller->get_CoreWebView2(&m_webview);

                            RECT bounds;
                            GetClientRect(m_parent, &bounds);
                            m_controller->put_Bounds(bounds);

                            HookDownloadEvents();
                            m_webview->Navigate(m_homeUrl.c_str());
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    return SUCCEEDED(hr);
}

void BrowserPanel::HookDownloadEvents() {
    if (!m_webview) return;

    // WebView2 exposes download events via the experimental
    // ICoreWebView2DownloadStartingEventHandler on newer SDK versions.
    // Cast through the environment-specific interface at runtime.
    wil::com_ptr<ICoreWebView2_4> webview4;
    if (SUCCEEDED(m_webview->QueryInterface(IID_PPV_ARGS(&webview4)))) {
        EventRegistrationToken token;
        webview4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                    wil::com_ptr<ICoreWebView2DownloadOperation> download;
                    args->get_DownloadOperation(&download);

                    // Redirect the save path into our soundpacks folder, keep original filename.
                    LPWSTR suggestedName = nullptr;
                    download->get_ResultFilePath(&suggestedName);
                    std::wstring fileName = suggestedName ? PathFindFileNameW(suggestedName) : L"soundpack_download";
                    std::wstring targetPath = m_downloadFolder + L"\\" + fileName;
                    args->put_ResultFilePath(targetPath.c_str());
                    args->put_Handled(TRUE);

                    // Own token per download operation (can't reuse the outer
                    // add_DownloadStarting token; also more correct, since each
                    // in-flight download needs its own registration).
                    EventRegistrationToken stateChangedToken;
                    download->add_StateChanged(
                        Callback<ICoreWebView2StateChangedEventHandler>(
                            [this, targetPath](ICoreWebView2DownloadOperation* op, IUnknown*) -> HRESULT {
                                COREWEBVIEW2_DOWNLOAD_STATE state;
                                op->get_State(&state);
                                if (state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED && m_downloadCallback) {
                                    m_downloadCallback(targetPath);
                                }
                                return S_OK;
                            }).Get(),
                        &stateChangedToken);

                    return S_OK;
                }).Get(),
            &token);
    }
}

void BrowserPanel::Resize(int width, int height) {
    if (!m_controller) return;
    RECT bounds{0, 0, width, height};
    m_controller->put_Bounds(bounds);
}

void BrowserPanel::Navigate(const std::wstring& url) {
    if (m_webview) m_webview->Navigate(url.c_str());
}

void BrowserPanel::GoHome() {
    Navigate(m_homeUrl);
}
