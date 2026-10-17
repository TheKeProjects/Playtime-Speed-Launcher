#pragma once
#include <string>
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Tiny always-on-top, click-through HUD pinned to a screen corner showing "FPS: 144" for the
// game being tracked. Re-asserts topmost every 1.5 s (borderless games fight for it).
class FpsOverlayWindow : public UI::Window {
public:
    bool Open();
    void SetFps(double fps);
    void SetAppearance(const std::wstring& corner, float fontSize, bool poppyFont);

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void Place();
    std::wstring text = L"FPS: --";
    std::wstring corner = L"top-right";
    float fontSize = 18.f;
    bool poppy = true;
    float placedW = 0.f, placedH = 0.f;
    double lastTopmost = 0.0;
};

} // namespace SpeedrunLauncher::Windows
