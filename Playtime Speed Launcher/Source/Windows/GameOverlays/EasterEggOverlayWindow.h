#pragma once
#include <functional>
#include <string>
#include "Services/Media/VideoPlayer.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Plays an easter egg video over the game: always on top, click-through and never activated, so
// the game keeps keyboard focus.
class EasterEggOverlayWindow : public UI::Window {
public:
    explicit EasterEggOverlayWindow(std::wstring videoPath) : path(std::move(videoPath)) {}
    bool Open();
    std::function<void()> onFinished;   // video ended or failed

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    std::wstring path;
    Media::VideoPlayer player;
    double lastTopmost = 0.0;
    bool finished = false;
};

} // namespace SpeedrunLauncher::Windows
