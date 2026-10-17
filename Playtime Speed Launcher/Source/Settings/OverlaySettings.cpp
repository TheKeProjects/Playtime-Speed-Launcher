#include "Settings/OverlaySettings.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

Overlays& Overlays::Current() {
    static Overlays current = [] {
        Overlays o;
        o.controllerEnabled = Config::ReadFlag(L"overlay_enabled.cfg", false);
        if (auto v = Config::ReadText(L"overlay_controller.cfg")) o.controller = *v;
        if (auto v = Config::ReadText(L"overlay_corner.cfg")) o.controllerCorner = *v;
        if (auto v = Config::ReadText(L"overlay_keyboard_layout.cfg")) o.keyboardLayout = *v;
        o.fpsEnabled = Config::ReadFlag(L"fps_overlay_enabled.cfg", true);
        if (auto v = Config::ReadText(L"fps_overlay_corner.cfg")) o.fpsCorner = *v;
        if (auto v = Config::ReadText(L"fps_overlay_size.cfg")) o.fpsSize = *v;
        if (auto v = Config::ReadText(L"fps_overlay_font.cfg")) o.fpsFont = *v;
        return o;
    }();
    return current;
}

void Overlays::SaveControllerEnabled() const { Config::WriteFlag(L"overlay_enabled.cfg", controllerEnabled); }
void Overlays::SaveController() const { Config::WriteText(L"overlay_controller.cfg", controller); }
void Overlays::SaveControllerCorner() const { Config::WriteText(L"overlay_corner.cfg", controllerCorner); }
void Overlays::SaveKeyboardLayout() const { Config::WriteText(L"overlay_keyboard_layout.cfg", keyboardLayout); }
void Overlays::SaveFpsEnabled() const { Config::WriteFlag(L"fps_overlay_enabled.cfg", fpsEnabled); }
void Overlays::SaveFpsCorner() const { Config::WriteText(L"fps_overlay_corner.cfg", fpsCorner); }
void Overlays::SaveFpsSize() const { Config::WriteText(L"fps_overlay_size.cfg", fpsSize); }
void Overlays::SaveFpsFont() const { Config::WriteText(L"fps_overlay_font.cfg", fpsFont); }

} // namespace SpeedrunLauncher::Settings
