#include "Controls/YouTubePlayer.h"
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Controls {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace {

constexpr const wchar_t* kVirtualHost = L"playtimeturbo.local";

// Hides the embed's own chrome (pause card, end screen, suggestion cards) and drives the title
// bar / watermark on a faster idle timer. Covers both the classic .ytp-* player and the newer
// web-components build.
constexpr const wchar_t* kHideChromeScript = LR"js((function() {
  var s = document.createElement('style');
  s.textContent =
    'player-fullscreen-controls, ' +
    '.ytp-pause-overlay, .ytp-ce-element, .ytp-ce-video, .ytp-ce-playlist, ' +
    '.ytp-ce-channel, .ytp-endscreen-content, .ytp-cards-teaser, .ytp-gradient-bottom ' +
    '{ display: none !important; }';
  (document.head || document.documentElement).appendChild(s);
  var IDLE_MS = 900;
  var lastActive = Date.now();
  document.addEventListener('mousemove', function () { lastActive = Date.now(); }, true);
  document.addEventListener('mousedown', function () { lastActive = Date.now(); }, true);
  setInterval(function () {
    var idle = (Date.now() - lastActive) >= IDLE_MS;
    var els = document.querySelectorAll(
      'player-top-controls, player-middle-controls, ' +
      '.ytp-chrome-top, .ytp-title, .ytp-watermark, .ytp-gradient-top, .ytp-large-play-button');
    for (var i = 0; i < els.length; i++) {
      els[i].style.setProperty('transition', 'opacity 0.08s linear', 'important');
      if (idle) els[i].style.setProperty('opacity', '0', 'important');
      else els[i].style.removeProperty('opacity');
    }
  }, 200);
})();)js";

constexpr const char* kPlayerHtml = R"html(<!DOCTYPE html>
<html><head><style>html,body{margin:0;padding:0;background:#000;overflow:hidden;height:100%;}#player{width:100%;height:100%;}</style></head>
<body>
<div id="player"></div>
<script>
  var tag = document.createElement('script');
  tag.src = "{IFRAME_API}";
  document.body.appendChild(tag);

  var player = null;
  var ready = false;
  var pendingVideoId = null;

  function post(obj) { window.chrome.webview.postMessage(JSON.stringify(obj)); }

  function onYouTubeIframeAPIReady() {
    player = new YT.Player('player', {
      width: '100%', height: '100%',
      playerVars: {
        autoplay: 1, playsinline: 1, modestbranding: 1,
        rel: 0, controls: 0, fs: 0, disablekb: 1, iv_load_policy: 3,
        origin: 'https://playtimeturbo.local'
      },
      events: {
        onReady: function() {
          ready = true;
          post({ type: 'ready' });
          if (pendingVideoId) { player.loadVideoById(pendingVideoId); pendingVideoId = null; }
        },
        onStateChange: function(e) { post({ type: 'state', state: e.data }); },
        onError: function(e) { post({ type: 'error', code: e.data }); }
      }
    });
  }

  function loadVideo(id) {
    if (ready && player && player.loadVideoById) player.loadVideoById(id);
    else pendingVideoId = id;
  }
  function playVideo()  { if (ready) player.playVideo(); }
  function pauseVideo() { if (ready) player.pauseVideo(); }
  function stopVideo()  { if (ready) player.stopVideo(); }
  function seekTo(sec)  { if (ready) player.seekTo(sec, true); }
  function setVolume(v) { if (ready) player.setVolume(Math.round(v * 100)); }

  setInterval(function() {
    if (!ready || !player || !player.getCurrentTime) return;
    try { post({ type: 'time', current: player.getCurrentTime(), duration: player.getDuration(), state: player.getPlayerState() }); }
    catch (e) {}
  }, 250);
</script>
</body></html>
)html";

std::wstring Number(double v) {
    wchar_t buf[64];
    swprintf_s(buf, L"%.3f", v);   // "C" locale: '.' decimal separator
    return buf;
}

std::wstring DescribeError(int code) {
    switch (code) {
        case 2: return L"This video's link looks invalid.";
        case 5: return L"This video can't be played in an embedded player.";
        case 100: return L"This video was removed or made private.";
        case 101:
        case 150: return L"The video owner disabled playback outside of YouTube.";
        default: return L"Playback error (" + std::to_wstring(code) + L").";
    }
}

// The WebView lives in its own borderless popup owned by the window it decorates: a child HWND
// isn't composited inside our transparent (WS_EX_NOREDIRECTIONBITMAP / DirectComposition) windows.
LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;   // clicks don't steal focus from the owner
    if (msg == WM_SIZE) {
        auto* controller = reinterpret_cast<ICoreWebView2Controller*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (controller) {
            RECT r{ 0, 0, LOWORD(lp), HIWORD(lp) };
            controller->put_Bounds(r);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND CreateHost(HWND owner) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = HostProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"SpeedrunLauncher.WebViewHost";
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        RegisterClassW(&wc);
        registered = true;
    }
    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"SpeedrunLauncher.WebViewHost", L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
}

} // namespace

struct YouTubePlayer::Impl {
    HWND parent = nullptr;
    HWND host = nullptr;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

YouTubePlayer::YouTubePlayer() : impl(std::make_unique<Impl>()) {}

YouTubePlayer::~YouTubePlayer() { Destroy(); }

void YouTubePlayer::Create(HWND parent) {
    impl->parent = parent;
    impl->host = CreateHost(parent);
    const std::wstring appData = Files::Combine(Files::LocalAppData(), L"PlaytimeSpeedLauncher");
    const std::wstring userData = Files::Combine(appData, L"WebView2");
    const std::wstring site = Files::Combine(appData, L"WebView2Player");
    Files::CreateDirectories(site);
    std::string html = kPlayerHtml;
    const std::string apiToken = "{IFRAME_API}";
    html.replace(html.find(apiToken), apiToken.size(), Text::Narrow(Config::Links::YouTubeIframeApi));   // URL from App/Config.h
    Files::WriteBytes(Files::Combine(site, L"player.html"), html);

    // Without this the video uses a hardware overlay plane that window capture can't see
    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--disable-features=DirectCompositionVideoOverlays");

    auto guard = alive;
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([this, guard, site](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (!*guard) return S_OK;
            if (FAILED(result) || !env) {
                if (onError) onError(L"Could not start the embedded video player. Make sure the WebView2 Runtime is installed.");
                return S_OK;
            }
            return env->CreateCoreWebView2Controller(
                impl->host,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([this, guard, site](HRESULT r, ICoreWebView2Controller* controller) -> HRESULT {
                    if (!*guard) {
                        if (controller) controller->Close();
                        return S_OK;
                    }
                    if (FAILED(r) || !controller) {
                        if (onError) onError(L"Could not start the embedded video player. Make sure the WebView2 Runtime is installed.");
                        return S_OK;
                    }
                    impl->controller = controller;
                    SetWindowLongPtrW(impl->host, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
                    controller->get_CoreWebView2(&impl->webview);
                    // Black behind the page while it loads
                    ComPtr<ICoreWebView2Controller2> c2;
                    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&c2)))) c2->put_DefaultBackgroundColor({ 255, 0, 0, 0 });
                    RECT client{};
                    GetClientRect(impl->host, &client);
                    controller->put_Bounds(client);
                    controller->put_IsVisible(TRUE);

                    ComPtr<ICoreWebView2Settings> settings;
                    if (SUCCEEDED(impl->webview->get_Settings(&settings))) {
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(FALSE);
                    }
                    // The IFrame API rejects NavigateToString's opaque origin (error 153)
                    ComPtr<ICoreWebView2_3> wv3;
                    if (SUCCEEDED(impl->webview->QueryInterface(IID_PPV_ARGS(&wv3))))
                        wv3->SetVirtualHostNameToFolderMapping(kVirtualHost, site.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

                    EventRegistrationToken token{};
                    impl->webview->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>([this, guard](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            if (!*guard) return S_OK;
                            LPWSTR message = nullptr;
                            if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                                OnMessage(message);
                                CoTaskMemFree(message);
                            }
                            return S_OK;
                        }).Get(),
                        &token);

                    // Re-inject the chrome-hiding CSS into the YouTube iframe on every load
                    ComPtr<ICoreWebView2_4> wv4;
                    if (SUCCEEDED(impl->webview->QueryInterface(IID_PPV_ARGS(&wv4)))) {
                        wv4->add_FrameCreated(
                            Callback<ICoreWebView2FrameCreatedEventHandler>([](ICoreWebView2*, ICoreWebView2FrameCreatedEventArgs* args) -> HRESULT {
                                ComPtr<ICoreWebView2Frame> frame;
                                if (FAILED(args->get_Frame(&frame))) return S_OK;
                                ComPtr<ICoreWebView2Frame2> frame2;
                                if (FAILED(frame.As(&frame2))) return S_OK;
                                EventRegistrationToken t{};
                                frame2->add_DOMContentLoaded(
                                    Callback<ICoreWebView2FrameDOMContentLoadedEventHandler>(
                                        [frame2](ICoreWebView2Frame*, ICoreWebView2DOMContentLoadedEventArgs*) -> HRESULT {
                                            frame2->ExecuteScript(kHideChromeScript, nullptr);
                                            return S_OK;
                                        })
                                        .Get(),
                                    &t);
                                return S_OK;
                            }).Get(),
                            &token);
                    }
                    impl->webview->Navigate((std::wstring(L"https://") + kVirtualHost + L"/player.html").c_str());
                    return S_OK;
                }).Get());
        }).Get());
    if (FAILED(hr) && onError) onError(L"Could not start the embedded video player. Make sure the WebView2 Runtime is installed.");
}

void YouTubePlayer::Destroy() {
    *alive = false;
    StopTimeout();
    if (impl && impl->controller) {
        SetWindowLongPtrW(impl->host, GWLP_USERDATA, 0);
        impl->controller->Close();
        impl->controller.Reset();
        impl->webview.Reset();
    }
    if (impl && impl->host) {
        DestroyWindow(impl->host);
        impl->host = nullptr;
    }
}

void YouTubePlayer::SetBounds(const RECT& physical) {
    bounds = physical;
    if (!impl->host) return;
    POINT origin{ physical.left, physical.top };
    ClientToScreen(impl->parent, &origin);
    SetWindowPos(impl->host, nullptr, origin.x, origin.y, physical.right - physical.left, physical.bottom - physical.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void YouTubePlayer::SetVisible(bool on) {
    visible = on;
    if (impl->host) ShowWindow(impl->host, on ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void YouTubePlayer::Run(const std::wstring& script) {
    if (!playerReady && script.rfind(L"loadVideo", 0) != 0) return;
    if (impl->webview) impl->webview->ExecuteScript(script.c_str(), nullptr);
}

void YouTubePlayer::OnMessage(const std::wstring& json) {
    const Json::Value root = Json::Parse(std::wstring_view(json));
    const std::string& type = root["type"].Str();
    if (type == "ready") {
        playerReady = true;
        if (!pendingVideo.empty()) {
            Run(L"loadVideo('" + pendingVideo + L"')");
            pendingVideo.clear();
        }
        Run(L"setVolume(" + Number(volume) + L")");
    } else if (type == "time") {
        position = root["current"].Number();
        const double d = root["duration"].Number();
        const int state = static_cast<int>(root["state"].Int());
        if (d > 0.0 && d != duration) {
            duration = d;
            if (!firedOpened) {
                firedOpened = true;
                StopTimeout();
                if (onOpened) onOpened();
            }
        }
        if (state == 1 && onPlayState) onPlayState(true);
        else if (state == 2 && onPlayState) onPlayState(false);
    } else if (type == "state") {
        if (root["state"].Int() == 0 && onEnded) onEnded();
    } else if (type == "error") {
        StopTimeout();
        if (onError) onError(DescribeError(static_cast<int>(root["code"].Int())));
    }
}

void YouTubePlayer::LoadVideo(const std::wstring& videoId) {
    firedOpened = false;
    duration = 0.0;
    position = 0.0;
    StartTimeout();
    if (!playerReady) {
        pendingVideo = videoId;
        return;
    }
    pendingVideo.clear();
    Run(L"loadVideo('" + videoId + L"')");
}

void YouTubePlayer::Play() { Run(L"playVideo()"); }
void YouTubePlayer::Pause() { Run(L"pauseVideo()"); }

void YouTubePlayer::Stop() {
    firedOpened = false;
    duration = 0.0;
    position = 0.0;
    StopTimeout();
    Run(L"stopVideo()");
}

void YouTubePlayer::Seek(double seconds) {
    position = seconds;
    Run(L"seekTo(" + Number(seconds) + L")");
}

void YouTubePlayer::SetVolume(double v) {
    volume = v;
    Run(L"setVolume(" + Number(v) + L")");
}

void YouTubePlayer::StartTimeout() {
    StopTimeout();
    auto guard = alive;
    timeoutTimer = Async::After(15000, [this, guard] {
        if (!*guard) return;
        timeoutTimer = 0;
        if (onError) onError(L"Timed out loading this video. Try again or pick another one.");
    });
}

void YouTubePlayer::StopTimeout() {
    if (timeoutTimer) Async::CancelTimer(timeoutTimer);
    timeoutTimer = 0;
}

} // namespace SpeedrunLauncher::Controls
