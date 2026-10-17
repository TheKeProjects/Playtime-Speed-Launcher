#pragma once
#include <cstdint>
#include <memory>

namespace SpeedrunLauncher::Windows {
class ControllerOverlayWindow;
class FpsOverlayWindow;
} // namespace SpeedrunLauncher::Windows

namespace SpeedrunLauncher::Features {

// The windows drawn over the game: the controller input overlay and the FPS counter
// (tracking runs through the elevated --fps-helper process).
class GameOverlays {
public:
    GameOverlays();
    ~GameOverlays();

    // Controller overlay (re)created from the saved settings; closed when disabled.
    void ApplyController();
    void CloseController();

    void StartFps(uint32_t gamePid);
    void StopFps();
    void ApplyFpsAppearance();
    void Shutdown();

private:
    std::unique_ptr<Windows::ControllerOverlayWindow> controller;
    std::unique_ptr<Windows::FpsOverlayWindow> fps;
};

} // namespace SpeedrunLauncher::Features
