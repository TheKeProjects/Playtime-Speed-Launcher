#pragma once
#include <string>
#include "Services/Media/VideoPlayer.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Easter egg video inside the launcher (960×570 panel over a dark backdrop; any click closes it).
class EasterEggOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return visible; }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Play(const std::wstring& path);
    void Stop();

private:
    Media::VideoPlayer player;
    bool visible = false;
};

} // namespace SpeedrunLauncher::Main
