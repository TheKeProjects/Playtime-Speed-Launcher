#pragma once
#include <d2d1_3.h>
#include <atomic>
#include <memory>
#include <string>
#include <wrl/client.h>
#include "Graphics/Rect.h"

namespace SpeedrunLauncher {
class Graphics;
}

namespace SpeedrunLauncher::Media {

// Video playback through the Media Foundation Media Engine (replaces WPF's MediaElement).
// Frames are decoded on the GPU into a D3D11 texture shared with Direct2D, so drawing a frame
// is just a bitmap draw. Call Update() every frame while playing, then Draw().
class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    bool Open(const std::wstring& path, bool autoPlay = true, bool loop = false);
    void Play();
    void Pause();
    void Stop();                         // releases the engine
    bool IsOpen() const;
    bool IsPlaying() const;
    bool Ended() const { return ended; }
    bool Failed() const { return failed; }

    void SetVolume(double volume);       // 0..1
    void SetMuted(bool muted);
    bool Muted() const { return muted; }
    double Position() const;             // seconds
    double Duration() const;             // seconds (0 while unknown)
    void Seek(double seconds);

    // Copies a new frame if one is ready; returns true when the image changed.
    bool Update();
    // Uniform letterboxed draw (Stretch="Uniform").
    void Draw(Graphics& g, const Rect& r, float opacity = 1.f);
    float VideoWidth() const { return static_cast<float>(width); }
    float VideoHeight() const { return static_cast<float>(height); }

    // Engine callbacks
    void OnEvent(unsigned long event, unsigned long long param1, unsigned long param2);

private:
    bool EnsureTarget();

    struct Impl;
    std::unique_ptr<Impl> impl;
    unsigned width = 0, height = 0;
    std::atomic_bool ended{ false }, failed{ false }, ready{ false };
    bool muted = false;
    bool loop = false;
    double volume = 1.0;
};

} // namespace SpeedrunLauncher::Media
