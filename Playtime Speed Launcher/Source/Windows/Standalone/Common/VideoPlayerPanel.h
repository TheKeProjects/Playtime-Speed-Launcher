#pragma once
#include <functional>
#include <memory>
#include <string>
#include "Controls/YouTubePlayer.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Windows {

// The video area of the tutorial windows: the embedded YouTube player (a WebView2 child window
// positioned over its rect), the empty / loading / error state, the teal seek bar and the
// controls row (play-pause, time, volume with mute toggle).
class VideoPlayerPanel {
public:
    struct Style {
        bool emptyGlyph = false;          // big ▶ above the empty-state label
        std::wstring emptyText;           // "Select a video"
        Color errorColor = Rgba(200, 80, 60, 200);
    };

    void Create(HWND parent, Style style);
    void Destroy() { player.Destroy(); }

    void Load(const std::wstring& videoId);
    void Stop();                      // back to the empty state
    void ShowError(const std::wstring& message);
    bool Loading() const { return loading; }
    bool HasVideo() const { return hasVideo; }
    void TogglePlay();

    std::function<void()> onOpened, onFailed;

    // `area` = video + 22 px seek row + 52 px controls row. `scale` = DIPs → pixels.
    void Draw(UI::Ui& ui, const Rect& area, float scale, bool roundBottomLeft, bool roundBottomRight, bool controlsBottomBorder);

private:
    Controls::YouTubePlayer player;
    Style style;
    bool hasVideo = false;   // a video is loaded (or loading)
    bool loading = false;
    bool playing = false;
    bool dragging = false;
    float seekValue = 0.f;   // 0..1 while dragging
    std::wstring message;    // empty-state label
    Color messageColor = Hex(0x1A3A55);
    float volume = 0.8f, volumeBeforeMute = 0.8f;
    bool muted = false;
    RECT lastBounds{ -1, -1, -1, -1 };
    bool lastVisible = false;
};

} // namespace SpeedrunLauncher::Windows
