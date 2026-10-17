#pragma once
#include <functional>
#include <memory>
#include "Services/Media/VideoPlayer.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

// Full-window intro video shown on startup; a click (or the end of the video) skips it.
class IntroVideo {
public:
    bool Start();                       // false when the video file is missing
    bool Visible() const { return visible; }
    void Hide();
    // Returns true when the intro finished / was skipped during this frame.
    bool Draw(UI::Ui& ui, const Rect& screen);

private:
    Media::VideoPlayer player;
    bool visible = false;
};

} // namespace SpeedrunLauncher::Main
