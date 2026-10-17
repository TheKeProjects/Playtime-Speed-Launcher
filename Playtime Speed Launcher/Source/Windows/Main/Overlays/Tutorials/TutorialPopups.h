#pragma once
#include <memory>
#include "Windows/Main/Overlays/Overlay.h"
#include "Windows/Popups/TutorialWindows.h"

namespace SpeedrunLauncher::Main {

// Owns the tutorial popup windows the Steam / Controller settings open (they are separate
// top-level windows, so nothing is drawn in the main window).
class TutorialPopups : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return false; }
    void Draw(UI::Ui& ui, const Rect& screen) override {
        (void)ui;
        (void)screen;
    }

    void ShowSteamTutorial();   // Steam launch option + controller restart steps
    void ShowVideoPopup();      // looping Tutorial.mp4

private:
    std::unique_ptr<Windows::TutorialVideoWindow> video;
    std::unique_ptr<Windows::TutorialStepsWindow> steps;
};

} // namespace SpeedrunLauncher::Main
