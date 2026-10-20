// Settings → Load Manip: chapter picker, then per chapter the Load Manip / FullBright keys
// (written straight into the mods' config.ini).
#include <map>
#include <optional>
#include <vector>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Features/InputRemaps.h"
#include "Input/HotkeyBinding.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Services/GameFiles/ModFiles.h"
#include "Settings/CoresSettings.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

namespace {

// One editable key of a mod config: its name in config.ini and the row label
struct KeySpec {
    const wchar_t* key;
    const char* label;
};

// Keys read from an installed mod config (KeyFreeze/[KeySlow/]KeyNormal or KeyUnlit/KeyLit).
struct ConfigKeys {
    std::optional<std::wstring> path;
    std::vector<std::optional<std::wstring>> values;
    bool Editable() const {
        if (values.empty()) return false;
        for (const auto& v : values)
            if (!v) return false;
        return true;
    }
};

struct CachedKeys {
    ConfigKeys keys;
    double readAt = -1e9;
};

std::map<int, CachedKeys>& Cache() {
    static std::map<int, CachedKeys> cache;
    return cache;
}

// Re-read at most every 1.5 s (the files live in the game folder and change outside the launcher).
const ConfigKeys& ReadKeys(Ui& ui, int chapter, bool fullBright, const std::vector<KeySpec>& specs) {
    auto& slot = Cache()[chapter * 2 + (fullBright ? 1 : 0)];
    if (ui.Time() - slot.readAt < 1.5) return slot.keys;
    slot.readAt = ui.Time();
    slot.keys = {};
    auto& l = App::L();
    slot.keys.path = fullBright ? l.ActiveFullBrightConfigPath(chapter) : l.ActiveLoadManipConfigPath(chapter);
    if (slot.keys.path)
        for (const auto& spec : specs) slot.keys.values.push_back(Services::ModFiles::ReadConfigKey(*slot.keys.path, spec.key));
    return slot.keys;
}

std::optional<std::wstring> UnrealName(const CapturedInput& in) {
    switch (in.mouseButton) {
        case ::SpeedrunLauncher::Input::MouseButtons::Middle: return L"MIDDLE_MOUSE_BUTTON";
        case ::SpeedrunLauncher::Input::MouseButtons::XButton1: return L"XBUTTON_ONE";
        case ::SpeedrunLauncher::Input::MouseButtons::XButton2: return L"XBUTTON_TWO";
        default: break;
    }
    if (in.escape || !in.vk) return std::nullopt;
    return ::SpeedrunLauncher::Input::KeyNames::Unreal(in.vk);
}

// The game reads these keys from its config.ini only when it starts, so while that chapter is open
// they can't be changed: a card says so, with a button to close the game. Returns true while locked.
bool GameOpenLock(SettingsContext& c, int chapterNumber) {
    static std::map<int, double> closingSince;   // "close game" pressed (per chapter)
    auto& l = App::L();
    auto& ui = c.ui;
    auto& g = ui.g;
    const int index = l.IndexOf(chapterNumber);
    const bool running = index >= 0 && index < static_cast<int>(l.gameRunning.size()) && l.gameRunning[static_cast<size_t>(index)];
    if (!running) {
        closingSince.erase(chapterNumber);
        return false;
    }
    if (c.capture.Any()) c.capture.Cancel();
    ui.KeepAnimatingThrottled();   // notice the game closing (the watcher checks every 2 s)

    const bool closing = closingSince.count(chapterNumber) && Async::Now() - closingSince[chapterNumber] < 8.0;
    const Color amber = Rgba(255, 176, 32);
    TextStyle body = Mono(10.f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const std::wstring text = Loc::Get("loadmanip_game_open_body");
    const float textW = c.col.w - 16.f - 34.f - 14.f - 16.f;
    const float th = g.Measure(text, body, textW).h;
    const Rect r = c.col.Next(16.f + 20.f + 6.f + th + 14.f + 34.f + 16.f, 16.f);
    g.GlowRound(r, 10.f, amber, 12.f, 0.3f);
    g.Border(r, 10.f, amber.WithAlpha(0.08f), amber.WithAlpha(0.55f), 1.f);
    const Rect icon{ r.x + 16.f, r.y + 16.f, 34.f, 34.f };
    g.FillRound(icon, 17.f, amber.WithAlpha(0.18f));
    Glyph(ui, 0xE72E, icon, 15.f, amber);   // lock
    const float tx = icon.R() + 14.f;
    g.Text(Loc::Fmt("loadmanip_game_open_title", chapterNumber), { tx, r.y + 16.f, textW, 20.f }, Hex(0xE1E8F0), Mono(11.f, true));
    g.Text(text, { tx, r.y + 16.f + 20.f + 6.f, textW, th }, Rgba(175, 198, 218, 230), body);

    const Rect b{ r.R() - 16.f - 170.f, r.B() - 16.f - 34.f, 170.f, 34.f };
    if (closing) {
        Form::Spinner(ui, b.x + 18.f, b.CY(), 8.f, amber);
        g.Text(Loc::Get("loadmanip_closing"), { b.x + 36.f, b.y, b.w - 36.f, b.h }, amber, Mono(10.f, true));
    } else {
        const Id id = ui.MakeId("lm-close-game", chapterNumber);
        const float hover = ui.Animate(HashId(id, 1), ui.Hot(id) ? 1.f : 0.f, 14.f);
        g.GlowRound(b, 8.f, Rgba(225, 65, 65), 12.f, 0.2f + 0.5f * hover);
        ButtonScope button(ui, id, b, { Rgba(215, 55, 55), Colors::Transparent, 0.f, 8.f, { 0.f }, 0.f, 34.f });
        const std::wstring label = Loc::Get("loadmanip_close_game");
        const TextStyle ls = Mono(10.f, true);
        const float lw = g.Measure(label, ls).w;
        const float gx = b.CX() - (12.f + 8.f + lw) * 0.5f;
        Glyph(ui, 0xE711, { gx, b.y, 12.f, b.h }, 10.f, Colors::White);   // ✕
        g.Text(label, { gx + 20.f, b.y, lw + 2.f, b.h }, Colors::White, ls);
        if (button.Clicked()) {
            // The watched process and its shipping binary (the launcher stub may have spawned it)
            if (index < static_cast<int>(l.runningPid.size()) && l.runningPid[static_cast<size_t>(index)]) Process::Kill(l.runningPid[static_cast<size_t>(index)]);
            if (auto exe = l.ActiveExePath(l.chapters[static_cast<size_t>(index)]))
                if (auto shipping = l.ShippingExeName(*exe))
                    for (uint32_t pid : Process::FindByName(*shipping)) Process::Kill(pid);
            closingSince[chapterNumber] = Async::Now();
        }
    }
    return true;
}

// One row per key editing it in the mod config, or the "not installed" note. Rows use the capture
// targets firstTarget, firstTarget + 1, ... `locked`: shown greyed out and not clickable (the game is open).
void ConfigKeyRows(SettingsContext& c, int chapter, bool fullBright, int firstTarget, const std::vector<KeySpec>& specs,
                   const char* notInstalled, bool locked = false) {
    auto& ui = c.ui;
    const int count = static_cast<int>(specs.size());
    const ConfigKeys& keys = ReadKeys(ui, chapter, fullBright, specs);
    if (!keys.Editable()) {
        for (int i = 0; i < count; ++i)
            if (c.capture.Active(firstTarget + i)) c.capture.Cancel();
        c.Paragraph(Loc::Get(notInstalled), 10.f, Palette::Hint);
        return;
    }
    const float top = c.col.y;
    if (locked) ui.g.PushOpacity(0.35f);
    for (int i = 0; i < count; ++i) {
        if (i) c.Gap(8.f);
        const int target = firstTarget + i;
        const bool capturing = c.capture.Active(target);
        const std::wstring text = capturing ? Loc::Get("f11_remap_press_input") : ::SpeedrunLauncher::Input::KeyNames::ConfigValueDisplay(*keys.values[i]);
        if (c.KeyRow(ui.MakeId("config-key", target), Loc::Get(specs[i].label), text, capturing) && !locked) {
            const std::wstring path = *keys.path, key = specs[i].key;
            const int slot = chapter * 2 + (fullBright ? 1 : 0);
            c.capture.Toggle(target, true, [path, key, slot](const CapturedInput& in) {
                if (auto name = UnrealName(in)) Services::ModFiles::UpdateConfigKey(path, key, *name);
                Cache()[slot].readAt = -1e9;
            });
        }
    }
    if (locked) {
        ui.Block({ c.col.x, top, c.col.w, c.col.y - top });   // nothing under it can be hovered / clicked
        ui.g.PopLayer();
    }
}

void Chapter1(SettingsContext& c) {
    const bool locked = GameOpenLock(c, 1);
    c.Section(Loc::Get("chapter1_section"));
    ConfigKeyRows(c, 1, false, CaptureCh1Freeze,
                  { { L"KeyFreeze", "chapter1_freeze_label" }, { L"KeyNormal", "chapter1_normal_label" } },
                  "chapter1_loadmanip_not_installed", locked);
    c.Rule();
    c.Section(Loc::Get("chapter1_fullbright_section"));
    ConfigKeyRows(c, 1, true, CaptureCh1Unlit,
                  { { L"KeyUnlit", "chapter1_fullbright_unlit_label" }, { L"KeyLit", "chapter1_fullbright_lit_label" } },
                  "chapter1_fullbright_not_installed", locked);
    c.Paragraph(Loc::Get("chapter1_hint"), 9.f, Palette::Dim, 8.f);
}

void Chapter4(SettingsContext& c) {
    const bool locked = GameOpenLock(c, 4);
    c.Section(Loc::Get("chapter4_section"));
    ConfigKeyRows(c, 4, false, CaptureCh4Freeze,
                  { { L"KeyFreeze", "chapter4_freeze_label" }, { L"KeySlow", "chapter4_slow_label" }, { L"KeyNormal", "chapter4_normal_label" } },
                  "chapter4_loadmanip_not_installed", locked);
    c.Rule();
    c.Section(Loc::Get("chapter4_fullbright_section"));
    ConfigKeyRows(c, 4, true, CaptureCh4Unlit,
                  { { L"KeyUnlit", "chapter4_fullbright_unlit_label" }, { L"KeyLit", "chapter4_fullbright_lit_label" } },
                  "chapter4_fullbright_not_installed", locked);
}

void Chapter5(SettingsContext& c) {
    const bool locked = GameOpenLock(c, 5);
    c.Section(Loc::Get("chapter5_section"));
    ConfigKeyRows(c, 5, false, CaptureCh5Freeze,
                  { { L"KeyFreeze", "chapter5_freeze_label" }, { L"KeyNormal", "chapter5_normal_label" } },
                  "chapter5_loadmanip_not_installed", locked);
    c.Rule();
    c.Section(Loc::Get("chapter5_fullbright_section"));
    ConfigKeyRows(c, 5, true, CaptureCh5Unlit,
                  { { L"KeyUnlit", "chapter5_fullbright_unlit_label" }, { L"KeyLit", "chapter5_fullbright_lit_label" } },
                  "chapter5_fullbright_not_installed", locked);
}

} // namespace

void LoadManip(SettingsContext& c) {
    auto& ui = c.ui;
    c.Section(Loc::Get("loadmanip_controls_section"));
    if (c.loadManipPage == 0) {
        if (c.Option(ui.MakeId("lm-nav", 1), Loc::Get("chapter1_section"), false, 8.f)) c.loadManipPage = 1;
        if (c.Option(ui.MakeId("lm-nav", 4), Loc::Get("chapter4_section"), false, 8.f)) c.loadManipPage = 2;
        if (c.Option(ui.MakeId("lm-nav", 5), Loc::Get("chapter5_section"), false)) c.loadManipPage = 3;
        return;
    }
    if (c.BackButton(ui.MakeId("lm-back"), Loc::Get("back"))) {
        c.capture.Cancel();
        c.loadManipPage = 0;
        return;
    }
    if (c.loadManipPage == 1) Chapter1(c);
    else if (c.loadManipPage == 2) Chapter4(c);
    else Chapter5(c);
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
