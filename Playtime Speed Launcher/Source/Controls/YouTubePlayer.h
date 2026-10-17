#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

namespace SpeedrunLauncher::Controls {

// YouTube's official IFrame player hosted in WebView2 (a child HWND over our Direct2D window).
// The page is served from a virtual host so the IFrame API accepts its origin, and the player
// reports state / time / errors back through postMessage.
class YouTubePlayer {
public:
    YouTubePlayer();
    ~YouTubePlayer();
    YouTubePlayer(const YouTubePlayer&) = delete;
    YouTubePlayer& operator=(const YouTubePlayer&) = delete;

    std::function<void()> onOpened, onEnded;
    std::function<void(const std::wstring&)> onError;
    std::function<void(bool playing)> onPlayState;

    // Starts WebView2 asynchronously inside `parent`. Errors come through onError.
    void Create(HWND parent);
    void Destroy();
    void SetBounds(const RECT& physical);   // client coordinates of the parent, in pixels
    void SetVisible(bool visible);

    void LoadVideo(const std::wstring& videoId);
    void Play();
    void Pause();
    void Stop();
    void Seek(double seconds);
    void SetVolume(double volume);   // 0..1

    bool HasDuration() const { return duration > 0.0; }
    double Duration() const { return duration; }
    double Position() const { return position; }

private:
    struct Impl;
    void Run(const std::wstring& script);
    void OnMessage(const std::wstring& json);
    void StartTimeout();
    void StopTimeout();

    std::unique_ptr<Impl> impl;
    bool playerReady = false;
    std::wstring pendingVideo;
    double duration = 0.0, position = 0.0, volume = 0.8;
    bool firedOpened = false;
    unsigned timeoutTimer = 0;
    RECT bounds{};
    bool visible = true;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace SpeedrunLauncher::Controls
