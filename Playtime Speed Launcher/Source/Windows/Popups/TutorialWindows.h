#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Services/Media/VideoPlayer.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Looping tutorial video (Assets\Videos\Tutorial.mp4) shown after copying the Steam launch option;
// any click or Esc closes it.
class TutorialVideoWindow : public UI::Window {
public:
    bool Open(HWND owner);

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Rgba(9, 12, 30); }

private:
    Media::VideoPlayer player;
};

// Step-by-step image tutorial (Steam launch option / controller setup) with Prev / n of N / Next.
class TutorialStepsWindow : public UI::Window {
public:
    struct Step {
        std::optional<std::wstring> image;
        std::optional<std::wstring> caption;
        bool copyButton = false;       // "copy the launch option" button
        bool controllerIcon = false;   // big gamepad glyph instead of a screenshot
    };
    bool Open(HWND owner, std::vector<Step> steps);

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Rgba(9, 12, 30); }

private:
    std::vector<Step> steps;
    size_t step = 0;
    bool copied = false;
};

// Centres a popup window of `width`×`height` DIPs on `owner` (WindowStartupLocation.CenterOwner).
void CenterOnOwner(UI::Window& window, HWND owner, float width, float height);

} // namespace SpeedrunLauncher::Windows
