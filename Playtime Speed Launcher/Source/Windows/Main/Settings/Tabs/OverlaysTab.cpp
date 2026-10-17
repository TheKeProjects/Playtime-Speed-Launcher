// Settings → Overlays: the controller input overlay and the FPS counter.
#include "App/Launcher.h"
#include "Features/GameOverlays.h"
#include "Localization/Localization.h"
#include "Settings/OverlaySettings.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

namespace {

const std::wstring kCorners[] = { L"top-left", L"top-right", L"bottom-left", L"bottom-right" };

int IndexOf(const std::wstring* values, int count, const std::wstring& value) {
    for (int i = 0; i < count; ++i)
        if (values[i] == value) return i;
    return -1;
}

std::vector<std::wstring> CornerTexts() {
    return { Loc::Get("overlay_corner_topleft"), Loc::Get("overlay_corner_topright"), Loc::Get("overlay_corner_bottomleft"),
             Loc::Get("overlay_corner_bottomright") };
}

void ControllerSection(SettingsContext& c, Settings::Overlays& s, Features::GameOverlays& overlays) {
    auto& ui = c.ui;
    c.Section(Loc::Get("overlays_section"));
    if (c.ToggleRow(ui.MakeId("overlay-enable"), Loc::Get("overlay_enable_label"), s.controllerEnabled)) {
        s.controllerEnabled = !s.controllerEnabled;
        s.SaveControllerEnabled();
        overlays.ApplyController();
    }
    c.Gap(16.f);
    c.Rule(0.f, 16.f);

    // Skin
    c.Section(Loc::Get("overlay_controller_label"));
    struct Skin {
        const wchar_t* value;
        wchar_t glyph;
        const char* key;
        float marginBottom;
    };
    static constexpr Skin kSkins[] = { { L"dualsense", Glyphs::Gamepad, "overlay_dualsense", 8.f },
                                       { L"xbox-controller", Glyphs::Gamepad, "overlay_xbox", 0.f },
                                       { L"keyboard", Glyphs::Keyboard, "overlay_keyboard", 0.f } };
    for (int i = 0; i < 3; ++i) {
        const Skin& skin = kSkins[i];
        if (!c.GlyphOption(ui.MakeId("overlay-skin", i), skin.glyph, Loc::Get(skin.key), skin.marginBottom, s.controller == skin.value)) continue;
        s.controller = skin.value;
        s.SaveController();
        if (s.controllerEnabled) overlays.ApplyController();   // reload with the new skin
    }

    // Keyboard layout (keyboard + mouse skin only)
    if (s.controller == L"keyboard") {
        c.Gap(16.f);
        c.Section(Loc::Get("overlay_keyboard_layout_label"));
        static const std::wstring kLayouts[] = { L"qwerty", L"azerty" };
        const int clicked = c.OptionGrid("overlay-layout", { Loc::Get("overlay_keyboard_layout_qwerty"), Loc::Get("overlay_keyboard_layout_azerty") },
                                         IndexOf(kLayouts, 2, s.keyboardLayout), 2);
        if (clicked >= 0) {
            s.keyboardLayout = kLayouts[clicked];
            s.SaveKeyboardLayout();
            if (s.controllerEnabled) overlays.ApplyController();
        }
    }
    c.Rule();

    // Corner
    c.Section(Loc::Get("overlay_corner_label"));
    const int corner = c.OptionGrid("overlay-corner", CornerTexts(), IndexOf(kCorners, 4, s.controllerCorner), 2);
    if (corner >= 0) {
        s.controllerCorner = kCorners[corner];
        s.SaveControllerCorner();
        if (s.controllerEnabled) overlays.ApplyController();   // reposition
    }
}

void FpsSection(SettingsContext& c, Settings::Overlays& s, Features::GameOverlays& overlays) {
    auto& ui = c.ui;
    c.Section(Loc::Get("fps_overlay_section"));
    if (c.ToggleRow(ui.MakeId("fps-enable"), Loc::Get("fps_overlay_enable_label"), s.fpsEnabled)) {
        s.fpsEnabled = !s.fpsEnabled;
        s.SaveFpsEnabled();
        if (s.fpsEnabled) {
            // A tracked game may already be running: start now instead of at the next launch
            for (uint32_t pid : App::L().runningPid) {
                if (!pid) continue;
                overlays.StartFps(pid);
                break;
            }
        } else {
            overlays.StopFps();
        }
    }
    c.Gap(16.f);

    c.Section(Loc::Get("overlay_corner_label"));
    const int corner = c.OptionGrid("fps-corner", CornerTexts(), IndexOf(kCorners, 4, s.fpsCorner), 2, 16.f);
    if (corner >= 0) {
        s.fpsCorner = kCorners[corner];
        s.SaveFpsCorner();
        overlays.ApplyFpsAppearance();
    }

    c.Section(Loc::Get("fps_overlay_size_label"));
    static const std::wstring kSizes[] = { L"small", L"medium", L"large" };
    const int size = c.OptionGrid("fps-size", { Loc::Get("fps_overlay_size_small"), Loc::Get("fps_overlay_size_medium"), Loc::Get("fps_overlay_size_large") },
                                  IndexOf(kSizes, 3, s.fpsSize), 3, 16.f);
    if (size >= 0) {
        s.fpsSize = kSizes[size];
        s.SaveFpsSize();
        overlays.ApplyFpsAppearance();
    }

    c.Section(Loc::Get("fps_overlay_font_label"));
    static const std::wstring kFonts[] = { L"poppy-playtime", L"monospace" };
    const int font = c.OptionGrid("fps-font", { Loc::Get("fps_overlay_font_poppy"), Loc::Get("fps_overlay_font_monospace") },
                                  IndexOf(kFonts, 2, s.fpsFont), 2, 0.f, { Font::Poppy, Font::Mono }, { 13.f, 11.f });
    if (font >= 0) {
        s.fpsFont = kFonts[font];
        s.SaveFpsFont();
        overlays.ApplyFpsAppearance();
    }
}

} // namespace

void Overlays(SettingsContext& c) {
    auto& s = Settings::Overlays::Current();
    auto& overlays = c.main.GameOverlays();
    ControllerSection(c, s, overlays);
    c.Rule();
    FpsSection(c, s, overlays);
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
