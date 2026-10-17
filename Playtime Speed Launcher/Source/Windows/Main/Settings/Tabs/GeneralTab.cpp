// Settings → General: sound volume, language, window mode, online users counter.
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Audio/SoundEffects.h"
#include "Settings/HeartbeatSettings.h"
#include "Settings/HotkeySettings.h"
#include "Settings/WindowModeSettings.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

namespace {

struct Language {
    const wchar_t* code;
    const wchar_t* name;
};
constexpr Language kLanguages[] = { { L"es", L"Español" }, { L"en", L"English" } };

const wchar_t* LanguageName(const std::wstring& code) {
    for (const auto& l : kLanguages)
        if (code == l.code) return l.name;
    return code.c_str();
}

// 20×14 flags drawn pixel-exact like the old generated bitmaps.
void Flag(Graphics& g, const std::wstring& lang, float x, float y) {
    constexpr int W = 20, H = 14;
    if (lang == L"es") {
        g.FillRect({ x, y, W, H }, Argb(0xFFAA151B));
        g.FillRect({ x, y + H / 4, W, H * 3 / 4 - H / 4 }, Argb(0xFFF1BF00));
    } else if (lang == L"en") {
        const int stripeH = H / 7;
        for (int row = 0; row < H; ++row)
            g.FillRect({ x, y + row, W, 1.f }, (row / stripeH) % 2 == 0 ? Argb(0xFFB22234) : Argb(0xFFFFFFFF));
        g.FillRect({ x, y, W * 2 / 5, H * 4 / 7 }, Argb(0xFF3C3B6E));
    } else {
        g.FillRect({ x, y, W, H }, Argb(0x88888888));
    }
}

void LanguageSelector(SettingsContext& c, const Rect& row) {
    auto& ui = c.ui;
    auto& g = ui.g;
    const std::wstring current = Loc::CurrentLang();
    const TextStyle text = Mono(11.f);
    const float nameW = g.Measure(LanguageName(current), text).w;
    const float w = std::max(130.f, 10.f + nameW + 6.f + 20.f + 8.f + 10.f + 8.f + 2.f);
    const Rect r = RightAligned(row, w, 30.f);
    const Id id = ui.MakeId("lang-selector");

    ButtonStyle s{ Palette::ButtonBg, Palette::ControlBorder, 1.f, 4.f, { 10.f, 0.f, 8.f, 0.f }, 130.f, 30.f };
    {
        ButtonScope b(ui, id, r, s);
        float x = b.content.x;
        g.Text(LanguageName(current), { x, r.y, nameW + 2.f, r.h }, Palette::Label, text);
        x += nameW + 6.f;
        Flag(g, current, x, std::round(r.CY() - 7.f));
        x += 20.f + 8.f;
        // Chevron turns 180° while the dropdown is open
        const float angle = ui.Animate(ui.MakeId("lang-chevron"), c.langOpen ? 180.f : 0.f, 16.f);
        const Rect chevron{ x, r.CY() - 5.f, 10.f, 10.f };
        g.PushTransform(D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(chevron.CX(), chevron.CY())));
        g.Text(std::wstring(1, Glyphs::ChevronDown), chevron, Palette::Muted, IconFont(9.f));
        g.PopTransform();
        if (b.Clicked()) c.langOpen = !c.langOpen;
    }

    if (!c.langOpen) {
        ui.SetValue(ui.MakeId("lang-open"), 0.f);
        return;
    }
    // Dropdown under the button (Popup Placement=Bottom), grows downwards
    const float itemH = g.Measure(L"Ag", text).h + 16.f;
    const Rect list{ r.x, r.B(), std::max(130.f, r.w), itemH * 2.f + 2.f };
    const float open = ui.Tween(ui.MakeId("lang-open"), 1.f, 0.18f, 0.f);
    auto& main = c.main;
    bool* langOpen = &c.langOpen;
    ui.Defer([&ui, &g, &main, list, itemH, open, text, langOpen] {
        g.PushTransform(D2D1::Matrix3x2F::Scale(1.f, open, D2D1::Point2F(list.x, list.y)));
        g.PushOpacity(open);
        ui.Block(list);
        g.FillRound(list, 0.f, 0.f, 4.f, 4.f, Palette::PanelBg);
        g.Border(list, 0.f, Colors::Transparent, Palette::PanelBorder, 1.f);
        float y = list.y + 1.f;
        int index = 0;
        for (const auto& l : kLanguages) {
            const Rect item{ list.x + 1.f, y, list.w - 2.f, itemH };
            ButtonScope b(ui, ui.MakeId("lang-option", index++), item, Styles::Flat());
            g.Text(l.name, item.Inset(10.f, 0.f), Palette::Label, text);
            Flag(g, l.code, item.R() - 10.f - 20.f, std::round(item.CY() - 7.f));
            if (b.Clicked()) {
                *langOpen = false;
                if (Loc::CurrentLang() != l.code) {
                    Loc::Load(l.code);
                    main.ApplyLanguage();
                }
            }
            y += itemH;
        }
        g.PopLayer();
        g.PopTransform();
        // Click anywhere else closes it (StaysOpen="False")
        if (ui.In().pressed[0] && !ui.MouseOver(list)) *langOpen = false;
    });
}

} // namespace

void General(SettingsContext& c) {
    auto& ui = c.ui;
    auto& hotkeys = Settings::Hotkeys::Current();

    // Sound effects volume
    {
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(ui.g.Measure(L"Ag", Mono(12.f)).h, 8.f);
        ui.g.Text(Loc::Get("sound_effects"), row, Palette::Label, Mono(12.f));
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(hotkeys.volume * 100.f))) + L"%", row, Palette::Teal, Mono(11.f).Right());
        float percent = hotkeys.volume * 100.f;
        if (Slider(ui, ui.MakeId("volume"), c.col.Next(20.f, 16.f), percent, 0.f, 100.f, 1.f)) {
            hotkeys.volume = percent / 100.f;
            Services::Sfx::SetVolume(hotkeys.volume);
            hotkeys.SaveVolume();
        }
    }
    c.Rule(0.f, 16.f);

    // Language
    {
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(30.f);
        ui.g.Text(Loc::Get("language_label"), row, Palette::Label, Mono(12.f));
        LanguageSelector(c, row);
    }
    c.Rule();

    // Window mode
    c.Section(Loc::Get("window_mode_section"));
    auto& mode = Settings::WindowMode::Current();
    const auto setMode = [&](const wchar_t* value) {
        mode.mode = value;
        mode.Save();
        c.main.ApplyWindowMode();
    };
    if (c.Option(ui.MakeId("wm-actual"), Loc::Get("window_mode_actual"), mode.mode == L"actual", 8.f)) setMode(L"actual");
    if (c.Option(ui.MakeId("wm-windowed"), Loc::Get("window_mode_windowed"), mode.mode == L"windowed", 8.f)) setMode(L"windowed");
    c.Rule();

    // Online users counter (only hides it — the heartbeat keeps running)
    c.Section(Loc::Get("online_users_section"));
    auto& heartbeat = Settings::Heartbeat::Current();
    if (c.ToggleRow(ui.MakeId("online-toggle"), Loc::Get("online_users_show_label"), heartbeat.showCounter)) {
        heartbeat.showCounter = !heartbeat.showCounter;
        heartbeat.Save();
    }
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
