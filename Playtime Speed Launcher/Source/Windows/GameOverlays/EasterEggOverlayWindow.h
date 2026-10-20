#pragma once
#include <functional>
#include <string>
#include "Graphics/Images.h"
#include "Services/Media/VideoPlayer.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Plays an easter egg video (or shows a photo for a few seconds) over the game: always on top,
// click-through and never activated, so the game keeps keyboard focus.
class EasterEggOverlayWindow : public UI::Window {
public:
    EasterEggOverlayWindow(std::wstring mediaPath, bool isPhoto) : path(std::move(mediaPath)), photo(isPhoto) {}
    bool Open();
    std::function<void()> onFinished;   // video ended or failed, or the photo's time ran out

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    static constexpr double kPhotoSeconds = 5.0;

    std::wstring path;
    bool photo = false;
    Media::VideoPlayer player;
    ImagePtr image;
    double shownAt = -1.0;
    double lastTopmost = 0.0;
    bool finished = false;
};

} // namespace SpeedrunLauncher::Windows
