// Settings → Controls: the two global hotkeys and the F11 remap.
#include <windows.h>
#include <algorithm>
#include "App/Launcher.h"
#include "Features/GlobalHotkeys.h"
#include "Input/HotkeyBinding.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Settings/KeyClickSettings.h"
#include "Features/InputRemaps.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

namespace {

void F11Bindings(SettingsContext& c) {
    auto& ui = c.ui;
    auto& f11 = App::L().f11;
    const auto& bindings = f11.Bindings();
    c.Gap(8.f);
    for (size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0) c.Gap(8.f);
        const int target = CaptureF11Slot + static_cast<int>(i);
        const bool capturing = c.capture.Active(target);
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(30.f);
        const std::wstring label = bindings.size() > 1 ? Loc::Fmt("f11_remap_key_label_n", static_cast<int>(i) + 1) : Loc::Get("f11_remap_key_label");
        ui.g.Text(label, row, Palette::Label, Mono(12.f));

        // [ key ][×]
        const Rect remove{ row.R() - 26.f, row.y, 26.f, 30.f };
        const std::wstring value = capturing ? Loc::Get("f11_remap_press_input") : BindingText(bindings[i]);
        const float w = ButtonWidth(ui, value, Mono(10.f), Styles::KeyBind());
        if (KeyBindButton(ui, ui.MakeId("f11-key", static_cast<int64_t>(i)), { remove.x - 6.f - w, row.y, w, 30.f }, value, capturing)) {
            c.capture.Toggle(target, true, [i](const CapturedInput& in) {
                auto& service = App::L().f11;
                if (in.mouseButton) service.ReplaceBinding(i, ::SpeedrunLauncher::Input::Binding::Mouse(in.mouseButton));
                else if (!in.escape && in.vk != VK_F11) service.ReplaceBinding(i, ::SpeedrunLauncher::Input::Binding::Key(in.vk));
            });
        }
        const ButtonStyle removeStyle{ Rgba(180, 30, 30, 40), Colors::Transparent, 0.f, 3.f, { 0.f }, 0.f, 0.f };
        if (TextButton(ui, ui.MakeId("f11-remove", static_cast<int64_t>(i)), remove, L"\x00D7", Mono(14.f).Center(), Rgba(200, 60, 60, 200),
                       removeStyle)) {
            c.capture.Cancel();
            f11.RemoveBinding(i);
            return;   // the list changed under us
        }
    }
}

// Key to click: the on/off switch, CONFIGURE (opens the keyboard → mouse wizard) and one line per
// configured key: "[C] acts as LEFT CLICK · spam 20/s   ✎ ✕"
void KeyClickSection(SettingsContext& c) {
    auto& ui = c.ui;
    auto& g = ui.g;
    auto& settings = Settings::KeyClick::Current();

    c.Section(Loc::Get("keyclick_section"));
    if (c.ToggleRow(ui.MakeId("keyclick-enable"), Loc::Get("keyclick_enable_label"), settings.enabled)) {
        settings.enabled = !settings.enabled;
        settings.Save();
        c.main.Remaps().RefreshKeyClicks();
    }
    // Outside the games too?
    c.Gap(8.f);
    if (c.ToggleRow(ui.MakeId("keyclick-everywhere"), Loc::Get("keyclick_everywhere_label"), settings.everywhere)) {
        settings.everywhere = !settings.everywhere;
        settings.Save();
        c.main.Remaps().RefreshKeyClicks();
    }
    // Shortcut to turn it on / off from anywhere (also in-game): [ Ctrl + K ][×]
    c.Gap(8.f);
    {
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(30.f);
        g.Text(Loc::Get("keyclick_hotkey_label"), row, Palette::Label, Mono(12.f));
        const bool capturing = c.capture.Active(CaptureKeyClickToggle);
        const std::wstring value = capturing ? Loc::Get("hotkey_press_keys")
                                   : settings.toggleVk ? ::SpeedrunLauncher::Input::KeyNames::Hotkey(settings.toggleModifiers, settings.toggleVk)
                                                       : Loc::Get("keyclick_hotkey_none");
        const float clearW = settings.toggleVk ? 26.f + 6.f : 0.f;
        const float w = ButtonWidth(ui, value, Mono(10.f), Styles::KeyBind());
        if (KeyBindButton(ui, ui.MakeId("keyclick-hotkey"), { row.R() - clearW - w, row.y, w, 30.f }, value, capturing)) {
            auto* window = &c.main;
            c.capture.Toggle(CaptureKeyClickToggle, false, [window](const CapturedInput& in) {
                if (in.escape || !in.vk) return;
                auto& s = Settings::KeyClick::Current();
                s.toggleModifiers = in.modifiers;
                s.toggleVk = in.vk;
                s.Save();
                window->ApplyKeyClickHotkey();
            });
        }
        if (settings.toggleVk &&
            TextButton(ui, ui.MakeId("keyclick-hotkey-clear"), { row.R() - 26.f, row.y + 2.f, 26.f, 26.f }, L"\x00D7", Mono(14.f).Center(),
                       Rgba(200, 60, 60, 220), { Rgba(180, 30, 30, 40), Colors::Transparent, 0.f, 4.f, { 0.f }, 0.f, 0.f })) {
            c.capture.Cancel();
            settings.toggleModifiers = settings.toggleVk = 0;
            settings.Save();
            c.main.ApplyKeyClickHotkey();
        }
    }
    if (settings.toggleVk && !c.main.KeyClickHotkeyOk()) c.Paragraph(Loc::Get("keyclick_hotkey_taken"), 9.f, Rgba(255, 176, 32), 4.f);
    c.Gap(10.f);
    {
        auto reveal = c.Reveal();
        const Rect r = c.col.Next(38.f);
        const Id id = ui.MakeId("keyclick-configure");
        const float hover = ui.Animate(HashId(id, 1), ui.Hot(id) ? 1.f : 0.f, 14.f);
        g.GlowRound(r, 6.f, Palette::Teal, 12.f, 0.15f + 0.45f * hover);
        ButtonScope button(ui, id, r, { Palette::Teal.WithAlpha(0.14f + 0.1f * hover), Palette::Teal.WithAlpha(0.7f), 1.f, 6.f, { 0.f }, 0.f, 38.f });
        const std::wstring label = Loc::Get("keyclick_configure");
        const TextStyle ls = Mono(11.f, true);
        const float lw = g.Measure(label, ls).w;
        const float gx = r.CX() - (16.f + 10.f + lw) * 0.5f;
        Glyph(ui, 0xE765, { gx, r.y, 16.f, r.h }, 14.f, Palette::Teal);   // keyboard
        g.Text(label, { gx + 26.f, r.y, lw + 2.f, r.h }, Palette::Teal, ls);
        if (button.Clicked()) c.main.OpenKeyClickWizard(-1);
    }

    const char* actions[3] = { "keyclick_click_left", "keyclick_click_right", "keyclick_click_middle" };
    for (size_t i = 0; i < settings.bindings.size(); ++i) {
        const auto& b = settings.bindings[i];
        c.Gap(8.f);
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(36.f);
        const Id rowId = ui.MakeId("keyclick-row", static_cast<int64_t>(i));
        const Interaction it = ui.Interact(rowId, row, true, false);
        const float hover = ui.Animate(HashId(rowId, 1), it.hovered ? 1.f : 0.f, 16.f);
        g.PushOpacity(settings.enabled ? 1.f : 0.45f);
        g.Border(row, 6.f, Rgba(255, 255, 255, static_cast<int>(8 + 10 * hover)), Rgba(255, 255, 255, static_cast<int>(24 + 30 * hover)), 1.f);
        // [ key ] acts as ACTION · spam N/s
        const std::wstring keyName = ::SpeedrunLauncher::Input::KeyNames::Friendly(b.vk);
        const TextStyle keyStyle = Mono(11.f, true).Center();
        const float kw = std::max(34.f, g.Measure(keyName, keyStyle).w + 18.f);
        const Rect cap{ row.x + 8.f, row.CY() - 12.f, kw, 24.f };
        g.FillRound(cap.Offset(0.f, 2.f), 5.f, Rgba(0, 0, 0, 120));
        g.Border(cap, 5.f, Hex(0x13202D), Rgba(255, 255, 255, 50), 1.f);
        g.Text(keyName, cap, Hex(0xE1E8F0), keyStyle);
        float x = cap.R() + 10.f;
        const std::wstring actsAs = Loc::Get("keyclick_acts_as");
        const float aw = g.Measure(actsAs, Mono(10.f)).w;
        g.Text(actsAs, { x, row.y, aw + 2.f, row.h }, Palette::Dim, Mono(10.f));
        x += aw + 10.f;
        std::wstring action = Loc::Get(actions[std::clamp(b.button, 0, 2)]);
        g.Text(action, { x, row.y, 200.f, row.h }, Palette::Teal, Mono(11.f, true));
        x += g.Measure(action, Mono(11.f, true)).w + 10.f;
        if (b.spam && settings.spamUnlocked) {
            const std::wstring spamText = Loc::Fmt("keyclick_spam_short", b.clicksPerSecond);
            const TextStyle ss = Mono(9.f, true).Center();
            const float sw = g.Measure(spamText, ss).w + 16.f;
            g.FillRound({ x, row.CY() - 10.f, sw, 20.f }, 10.f, Rgba(255, 176, 32, 40));
            g.Text(spamText, { x, row.CY() - 10.f, sw, 20.f }, Rgba(255, 176, 32), ss);
        }
        g.PopLayer();
        // ✎ ✕
        const Rect remove{ row.R() - 6.f - 26.f, row.CY() - 13.f, 26.f, 26.f };
        const Rect edit{ remove.x - 6.f - 26.f, remove.y, 26.f, 26.f };
        if (TextButton(ui, ui.MakeId("keyclick-edit", static_cast<int64_t>(i)), edit, L"\xE70F", IconFont(11.f), Rgba(80, 170, 230, 230),
                       { Rgba(0, 140, 200, 40), Colors::Transparent, 0.f, 4.f, { 0.f }, 0.f, 0.f }) ||
            (it.clicked && ui.HotId() == rowId)) {
            c.main.OpenKeyClickWizard(static_cast<int>(i));
        }
        if (TextButton(ui, ui.MakeId("keyclick-remove", static_cast<int64_t>(i)), remove, L"\x00D7", Mono(14.f).Center(), Rgba(200, 60, 60, 220),
                       { Rgba(180, 30, 30, 40), Colors::Transparent, 0.f, 4.f, { 0.f }, 0.f, 0.f })) {
            settings.bindings.erase(settings.bindings.begin() + static_cast<long long>(i));
            settings.Save();
            c.main.Remaps().RefreshKeyClicks();
            return;   // the list changed under us
        }
    }
    c.Paragraph(Loc::Get(settings.everywhere ? "keyclick_hint_everywhere" : "keyclick_hint"), 9.f, Palette::Dim, 10.f);
}

} // namespace

void Controls(SettingsContext& c) {
    auto& ui = c.ui;
    auto& hotkeys = c.main.Hotkeys();

    c.Section(Loc::Get("controls_section"));
    {
        const bool capturing = c.capture.Active(CaptureCheckpoint);
        const std::wstring value = capturing ? Loc::Get("hotkey_press_keys")
                                             : ::SpeedrunLauncher::Input::KeyNames::Hotkey(hotkeys.CheckpointModifiers(), hotkeys.CheckpointVk());
        if (c.KeyRow(ui.MakeId("checkpoint-hotkey"), Loc::Get("checkpoint_hotkey_label"), value, capturing)) {
            auto* h = &hotkeys;
            c.capture.Toggle(CaptureCheckpoint, false, [h](const CapturedInput& in) {
                if (!in.escape) h->SetCheckpoint(in.modifiers, in.vk);
            });
        }
    }
    c.Gap(8.f);
    {
        const bool capturing = c.capture.Active(CaptureTutorial);
        const std::wstring value = capturing ? Loc::Get("hotkey_press_keys")
                                             : ::SpeedrunLauncher::Input::KeyNames::Hotkey(hotkeys.TutorialModifiers(), hotkeys.TutorialVk());
        if (c.KeyRow(ui.MakeId("tutorial-hotkey"), Loc::Get("tutorial_hotkey_label"), value, capturing)) {
            auto* h = &hotkeys;
            c.capture.Toggle(CaptureTutorial, false, [h](const CapturedInput& in) {
                if (!in.escape) h->SetTutorial(in.modifiers, in.vk);
            });
        }
    }
    c.Rule();

    // F11 remap
    auto& f11 = App::L().f11;
    c.Section(Loc::Get("f11_remap_section"));
    if (c.ToggleRow(ui.MakeId("f11-enable"), Loc::Get("f11_remap_enable_label"), f11.Enabled())) {
        c.capture.Cancel();
        f11.SetEnabled(!f11.Enabled());
    }
    F11Bindings(c);

    // "+ Add key" (right aligned, Margin top 8)
    c.Gap(8.f);
    {
        const bool adding = c.capture.Active(CaptureF11Add);
        const std::wstring text = adding ? Loc::Get("f11_remap_press_input") : Loc::Get("f11_remap_add_key");
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(30.f);
        const float w = ButtonWidth(ui, text, Mono(10.f), Styles::KeyBind());
        if (KeyBindButton(ui, ui.MakeId("f11-add"), RightAligned(row, w, 30.f), text, adding)) {
            c.capture.Toggle(CaptureF11Add, true, [](const CapturedInput& in) {
                auto& service = App::L().f11;
                if (in.mouseButton) service.AddBinding(::SpeedrunLauncher::Input::Binding::Mouse(in.mouseButton));
                else if (!in.escape && in.vk != VK_F11) service.AddBinding(::SpeedrunLauncher::Input::Binding::Key(in.vk));
            });
        }
    }
    c.Paragraph(Loc::Get("f11_remap_hint"), 9.f, Palette::Dim, 8.f);
    c.Rule();
    KeyClickSection(c);
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
