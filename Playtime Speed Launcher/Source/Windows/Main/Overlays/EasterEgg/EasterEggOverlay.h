#pragma once
#include <string>
#include "Graphics/Images.h"
#include "Services/Media/VideoPlayer.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Easter egg video or photo inside the launcher (960×570 panel over a dark backdrop; any click closes it).
class EasterEggOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return visible; }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Play(const std::wstring& path, bool photo = false);
    void Stop();

private:
    Media::VideoPlayer player;
    ImagePtr image;   // set for a photo (the player stays idle)
    bool visible = false;
};

} // namespace SpeedrunLauncher::Main
