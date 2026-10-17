#include "Features/GameOverlays.h"
#include "App/Launcher.h"
#include "Settings/OverlaySettings.h"
#include "Windows/GameOverlays/ControllerOverlayWindow.h"
#include "Windows/GameOverlays/FpsOverlayWindow.h"

namespace SpeedrunLauncher::Features {

GameOverlays::GameOverlays() {
    App::L().fps.SetCallback([this](double value) {
        if (fps) fps->SetFps(value);
    });
}

GameOverlays::~GameOverlays() { Shutdown(); }

void GameOverlays::ApplyController() {
    CloseController();
    const auto& s = Settings::Overlays::Current();
    if (!s.controllerEnabled) return;
    controller = std::make_unique<Windows::ControllerOverlayWindow>(s.controller, s.keyboardLayout);
    if (!controller->Open(s.controllerCorner)) controller.reset();
}

void GameOverlays::CloseController() {
    if (controller) {
        controller->Destroy();
        controller.reset();
    }
}

void GameOverlays::StartFps(uint32_t gamePid) {
    if (gamePid == 0 || !Settings::Overlays::Current().fpsEnabled) return;
    App::L().fps.Start(gamePid);
    if (!fps) {
        fps = std::make_unique<Windows::FpsOverlayWindow>();
        if (!fps->Open()) {
            fps.reset();
            return;
        }
        ApplyFpsAppearance();
    }
}

void GameOverlays::StopFps() {
    App::L().fps.Stop();
    App::L().fps.ClearDeclined();
    if (fps) {
        fps->Destroy();
        fps.reset();
    }
}

void GameOverlays::ApplyFpsAppearance() {
    if (!fps) return;
    const auto& s = Settings::Overlays::Current();
    fps->SetAppearance(s.fpsCorner, s.FpsFontSize(), s.fpsFont != L"monospace");
}

void GameOverlays::Shutdown() {
    CloseController();
    StopFps();
}

} // namespace SpeedrunLauncher::Features
