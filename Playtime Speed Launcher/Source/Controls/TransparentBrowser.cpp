#include "Controls/TransparentBrowser.h"
#include <dcomp.h>
#include <wrl.h>
#include <WebView2.h>
#include "App/Paths.h"

#pragma comment(lib, "dcomp.lib")

namespace SpeedrunLauncher::Controls {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

std::wstring Describe(HRESULT hr) {
    wchar_t code[16];
    swprintf_s(code, L"0x%08X", static_cast<unsigned>(hr));
    return std::wstring(L"Error ") + code;
}

} // namespace

struct TransparentBrowser::Impl {
    HWND parent = nullptr;
    ComPtr<IDCompositionDevice> dcomp;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;
    ComPtr<ICoreWebView2CompositionController> composition;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

TransparentBrowser::TransparentBrowser() : impl(std::make_unique<Impl>()) {}

TransparentBrowser::~TransparentBrowser() { Destroy(); }

void TransparentBrowser::Create(HWND parent) {
    impl->parent = parent;
    auto guard = alive;
    const auto fail = [this, guard](HRESULT hr) {
        if (*guard && onError) onError(Describe(hr));
    };

    // The window's composition tree: one visual the WebView2 renders into
    HRESULT hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&impl->dcomp));
    if (SUCCEEDED(hr)) hr = impl->dcomp->CreateTargetForHwnd(parent, TRUE, &impl->target);
    if (SUCCEEDED(hr)) hr = impl->dcomp->CreateVisual(&impl->visual);
    if (SUCCEEDED(hr)) hr = impl->target->SetRoot(impl->visual.Get());
    if (SUCCEEDED(hr)) hr = impl->dcomp->Commit();
    if (FAILED(hr)) return fail(hr);

    const std::wstring userData = Paths::DataFile(L"WebView2 Collectibles");
    hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([this, guard, fail](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (!*guard) return S_OK;
            ComPtr<ICoreWebView2Environment3> env3;
            if (FAILED(result) || !env || FAILED(env->QueryInterface(IID_PPV_ARGS(&env3)))) {
                fail(FAILED(result) ? result : E_NOINTERFACE);
                return S_OK;
            }
            return env3->CreateCoreWebView2CompositionController(
                impl->parent,
                Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                    [this, guard, fail](HRESULT r, ICoreWebView2CompositionController* composition) -> HRESULT {
                        ComPtr<ICoreWebView2Controller> controller;
                        if (composition) composition->QueryInterface(IID_PPV_ARGS(&controller));
                        if (!*guard) {
                            if (controller) controller->Close();
                            return S_OK;
                        }
                        if (FAILED(r) || !controller) {
                            fail(FAILED(r) ? r : E_FAIL);
                            return S_OK;
                        }
                        impl->composition = composition;
                        impl->controller = controller;
                        impl->composition->put_RootVisualTarget(impl->visual.Get());
                        impl->dcomp->Commit();
                        controller->get_CoreWebView2(&impl->webview);
                        controller->put_Bounds(bounds);
                        // Fully transparent where the page doesn't paint
                        if (ComPtr<ICoreWebView2Controller2> c2; SUCCEEDED(controller.As(&c2))) c2->put_DefaultBackgroundColor({ 0, 0, 0, 0 });
                        controller->put_IsVisible(TRUE);

                        ComPtr<ICoreWebView2Settings> settings;
                        if (SUCCEEDED(impl->webview->get_Settings(&settings))) {
                            settings->put_IsStatusBarEnabled(FALSE);
                            settings->put_IsZoomControlEnabled(FALSE);
                            settings->put_AreDefaultContextMenusEnabled(FALSE);
                            settings->put_AreDevToolsEnabled(FALSE);
                        }
                        if (!pendingUrl.empty()) {
                            impl->webview->Navigate(pendingUrl.c_str());
                            pendingUrl.clear();
                        }
                        return S_OK;
                    }).Get());
        }).Get());
    if (FAILED(hr)) fail(hr);
}

void TransparentBrowser::Destroy() {
    *alive = false;
    if (impl && impl->controller) {
        impl->controller->Close();
        impl->controller.Reset();
        impl->composition.Reset();
        impl->webview.Reset();
    }
    if (impl) {
        impl->visual.Reset();
        impl->target.Reset();
        impl->dcomp.Reset();
    }
}

void TransparentBrowser::SetBounds(const RECT& physical) {
    bounds = physical;
    if (impl->controller) impl->controller->put_Bounds(physical);
}

void TransparentBrowser::Navigate(const std::wstring& url) {
    if (impl->webview) impl->webview->Navigate(url.c_str());
    else pendingUrl = url;
}

} // namespace SpeedrunLauncher::Controls
