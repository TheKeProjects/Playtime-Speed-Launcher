#pragma once
#include <string>

namespace SpeedrunLauncher::Settings {

// Controller input overlay + FPS counter settings (one .cfg file per value, like before).
struct Overlays {
    // Controller overlay
    bool controllerEnabled = false;                  // overlay_enabled.cfg
    std::wstring controller = L"xbox-controller";    // overlay_controller.cfg: dualsense | xbox-controller | keyboard
    std::wstring controllerCorner = L"top-left";     // overlay_corner.cfg
    std::wstring keyboardLayout = L"qwerty";         // overlay_keyboard_layout.cfg: qwerty | azerty

    // FPS counter
    bool fpsEnabled = true;                          // fps_overlay_enabled.cfg
    std::wstring fpsCorner = L"top-right";           // fps_overlay_corner.cfg
    std::wstring fpsSize = L"medium";                // fps_overlay_size.cfg: small | medium | large
    std::wstring fpsFont = L"poppy-playtime";        // fps_overlay_font.cfg: poppy-playtime | monospace

    float FpsFontSize() const { return fpsSize == L"small" ? 14.f : fpsSize == L"large" ? 26.f : 18.f; }

    void SaveControllerEnabled() const;
    void SaveController() const;
    void SaveControllerCorner() const;
    void SaveKeyboardLayout() const;
    void SaveFpsEnabled() const;
    void SaveFpsCorner() const;
    void SaveFpsSize() const;
    void SaveFpsFont() const;

    static Overlays& Current();
};

} // namespace SpeedrunLauncher::Settings
