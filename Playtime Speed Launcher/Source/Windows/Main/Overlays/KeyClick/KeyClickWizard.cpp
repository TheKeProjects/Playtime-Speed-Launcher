#include "Windows/Main/Overlays/KeyClick/KeyClickWizard.h"
#include <windows.h>
#include <algorithm>
#include <vector>
#include "Core/Async.h"
#include "Features/InputRemaps.h"
#include "Graphics/Animation.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Services/Audio/SoundEffects.h"
#include "Settings/KeyClickSettings.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

constexpr Color kTitle = Rgba(225, 232, 240), kDim = Rgba(110, 140, 165);

struct Key {
    const wchar_t* label;
    unsigned vk;
    float units = 1.f;   // width in key units
};

// A compact US-style board: function row, numbers, letters, bottom row (Esc is reserved for "cancel")
const std::vector<std::vector<Key>>& Board() {
    static const std::vector<std::vector<Key>> rows = {
        { { L"Esc", 0 }, { L"", 0, 0.5f }, { L"F1", VK_F1 }, { L"F2", VK_F2 }, { L"F3", VK_F3 }, { L"F4", VK_F4 }, { L"", 0, 0.5f }, { L"F5", VK_F5 },
          { L"F6", VK_F6 }, { L"F7", VK_F7 }, { L"F8", VK_F8 }, { L"", 0, 0.5f }, { L"F9", VK_F9 }, { L"F10", VK_F10 }, { L"F11", VK_F11 }, { L"F12", VK_F12 } },
        { { L"`", VK_OEM_3 }, { L"1", '1' }, { L"2", '2' }, { L"3", '3' }, { L"4", '4' }, { L"5", '5' }, { L"6", '6' }, { L"7", '7' }, { L"8", '8' },
          { L"9", '9' }, { L"0", '0' }, { L"-", VK_OEM_MINUS }, { L"=", VK_OEM_PLUS }, { L"\x232B", VK_BACK, 2.f } },
        { { L"Tab", VK_TAB, 1.5f }, { L"Q", 'Q' }, { L"W", 'W' }, { L"E", 'E' }, { L"R", 'R' }, { L"T", 'T' }, { L"Y", 'Y' }, { L"U", 'U' }, { L"I", 'I' },
          { L"O", 'O' }, { L"P", 'P' }, { L"[", VK_OEM_4 }, { L"]", VK_OEM_6 }, { L"\\", VK_OEM_5, 1.5f } },
        { { L"Caps", VK_CAPITAL, 1.75f }, { L"A", 'A' }, { L"S", 'S' }, { L"D", 'D' }, { L"F", 'F' }, { L"G", 'G' }, { L"H", 'H' }, { L"J", 'J' },
          { L"K", 'K' }, { L"L", 'L' }, { L";", VK_OEM_1 }, { L"'", VK_OEM_7 }, { L"Enter", VK_RETURN, 2.25f } },
        { { L"Shift", VK_LSHIFT, 2.25f }, { L"Z", 'Z' }, { L"X", 'X' }, { L"C", 'C' }, { L"V", 'V' }, { L"B", 'B' }, { L"N", 'N' }, { L"M", 'M' },
          { L",", VK_OEM_COMMA }, { L".", VK_OEM_PERIOD }, { L"/", VK_OEM_2 }, { L"Shift", VK_RSHIFT, 2.75f } },
        { { L"Ctrl", VK_LCONTROL, 1.5f }, { L"Win", VK_LWIN, 1.25f }, { L"Alt", VK_LMENU, 1.25f }, { L"", VK_SPACE, 7.5f }, { L"Alt", VK_RMENU, 1.25f },
          { L"Ctrl", VK_RCONTROL, 1.75f } },
    };
    return rows;
}

// Left / right variants count as the same key (the window only reports "Shift", the hook "LShift")
unsigned Generic(unsigned vk) {
    switch (vk) {
        case VK_LSHIFT: case VK_RSHIFT: return VK_SHIFT;
        case VK_LCONTROL: case VK_RCONTROL: return VK_CONTROL;
        case VK_LMENU: case VK_RMENU: return VK_MENU;
        default: return vk;
    }
}

const char* ActionKey(int button) { return button == 1 ? "keyclick_click_right" : button == 2 ? "keyclick_click_middle" : "keyclick_click_left"; }

} // namespace

void KeyClickWizard::Open(int bindingIndex) {
    const auto& s = Settings::KeyClick::Current();
    editing = bindingIndex >= 0 && bindingIndex < static_cast<int>(s.bindings.size()) ? bindingIndex : -1;
    if (editing >= 0) {
        const auto& b = s.bindings[static_cast<size_t>(editing)];
        vk = b.vk;
        button = b.button;
        spam = b.spam;
        clicksPerSecond = b.clicksPerSecond;
    } else {
        vk = 0;
        button = 0;
        spam = false;
        clicksPerSecond = 15;
    }
    step = editing >= 0 ? 1 : 0;
    stepAt = Async::Now() + 0.12;   // after the panel's entrance
    stepFrom = 40.f;
    popup.Open();
}

void KeyClickWizard::SetStep(int s) {
    stepFrom = s > step ? 40.f : -40.f;
    step = s;
    stepAt = Async::Now();
}

bool KeyClickWizard::KeyTaken(unsigned key) const {
    const auto& s = Settings::KeyClick::Current();
    for (size_t i = 0; i < s.bindings.size(); ++i)
        if (static_cast<int>(i) != editing && Generic(s.bindings[i].vk) == Generic(key)) return true;
    return false;
}

void KeyClickWizard::ChooseKey(unsigned key) {
    if (!key || KeyTaken(key)) return;
    vk = key;
    SetStep(1);
}

void KeyClickWizard::Save() {
    auto& s = Settings::KeyClick::Current();
    Settings::KeyClickBinding b;
    b.vk = vk;
    b.button = button;
    b.spam = spam;
    b.clicksPerSecond = clicksPerSecond;
    if (editing >= 0 && editing < static_cast<int>(s.bindings.size())) s.bindings[static_cast<size_t>(editing)] = b;
    else s.bindings.push_back(b);
    s.enabled = true;   // configuring a key means wanting it on
    s.Save();
    main.Remaps().RefreshKeyClicks();
    popup.Close();
}

// ── Step 1: keyboard ────────────────────────────────────────────────────────

void KeyClickWizard::DrawKeyboard(Ui& ui, const Rect& area) {
    auto& g = ui.g;
    int item = 0;
    float y = area.y;
    {
        StaggerItem in(ui, stepAt, item++, stepFrom);
        g.Text(Loc::Get("keyclick_choose_key"), { area.x, y, area.w, 24.f }, kTitle, Mono(15.f, true).Center());
        g.Text(Loc::Get("keyclick_choose_key_hint"), { area.x, y + 26.f, area.w, 18.f }, kDim, Mono(10.f).Center());
    }
    y += 26.f + 18.f + 22.f;

    // Physical key presses pick too (Esc is "cancel")
    for (auto& k : ui.In().keys) {
        if (!k.down || k.handled || k.vk == VK_ESCAPE) continue;
        k.handled = true;
        Services::Sfx::PlayKeyPress();
        unsigned key = k.vk;
        if (key == VK_SHIFT) key = VK_LSHIFT;
        else if (key == VK_CONTROL) key = VK_LCONTROL;
        else if (key == VK_MENU) key = VK_LMENU;
        ChooseKey(key);
        return;
    }

    const auto& rows = Board();
    const float unit = 44.f, gap = 6.f;
    const float boardW = 15.f * unit + 14.f * gap;
    const float x0 = area.CX() - boardW * 0.5f;
    // The board's own plate
    const Rect plate{ x0 - 14.f, y - 14.f, boardW + 28.f, rows.size() * (unit + gap) - gap + 28.f };
    {
        StaggerItem in(ui, stepAt, item++, stepFrom);
        g.Shadow(plate.Offset(0.f, 8.f), 14.f, 18.f, Argb(0x80000000));
        g.Border(plate, 14.f, Hex(0x081019), Rgba(255, 255, 255, 26), 1.f);
    }
    for (size_t r = 0; r < rows.size(); ++r) {
        StaggerItem in(ui, stepAt, item++, 0.f, 18.f, 0.05);
        float x = x0;
        const float ky = y + static_cast<float>(r) * (unit + gap);
        for (size_t k = 0; k < rows[r].size(); ++k) {
            const Key& key = rows[r][k];
            const float w = key.units * unit + (key.units - 1.f) * gap;
            const Rect kr{ x, ky, w, unit };
            x += w + gap;
            if (!key.label[0] && !key.vk) continue;   // spacer
            const bool reserved = key.vk == 0;          // Esc
            const bool taken = !reserved && KeyTaken(key.vk);
            const bool current = vk && Generic(vk) == Generic(key.vk);
            const Id id = ui.MakeId("kc-key", static_cast<int64_t>(r * 100 + k));
            // Keys make a key sound when pressed instead of the usual hover / click sounds
            const Interaction it = ui.Interact(id, kr, !reserved && !taken, false);
            if (it.hovered && ui.In().pressed[0]) Services::Sfx::PlayKeyPress();
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 18.f);
            const float press = it.held ? 1.f : 0.f;
            const Rect cap = kr.Offset(0.f, -2.f * hover + 2.f * press);
            if (it.hovered) {
                ui.SetCursor(IDC_HAND);
                g.GlowRound(cap, 7.f, Palette::Teal, 12.f, 0.6f * hover);
            }
            // Keycap: darker base + lighter top face
            g.FillRound(kr.Offset(0.f, 3.f), 7.f, Rgba(0, 0, 0, 120));
            const Color face = current ? Palette::Teal : taken || reserved ? Hex(0x0D1620) : Hex(0x13202D);
            g.Border(cap, 7.f, face.WithAlpha(current ? 0.85f : 1.f),
                     hover > 0.01f ? Palette::Teal.WithAlpha(0.4f + 0.6f * hover) : Rgba(255, 255, 255, taken || reserved ? 14 : 34), 1.f);
            const Color text = current ? Rgba(3, 17, 13) : taken || reserved ? Rgba(120, 140, 160, 90) : kTitle;
            g.Text(key.label, cap, text, Mono(key.units > 1.4f ? 10.f : 12.f, true).Center());
            if (taken) {
                g.FillEllipse(cap.R() - 7.f, cap.y + 7.f, 2.5f, 2.5f, Rgba(255, 176, 32, 200));
                if (it.hovered || ui.MouseOver(kr)) ui.Tooltip(id, Loc::Get("keyclick_in_use"));
            }
            if (it.clicked && !taken && !reserved) {
                ChooseKey(key.vk);
                return;
            }
        }
    }
}

// ── Step 2: mouse ───────────────────────────────────────────────────────────

void KeyClickWizard::DrawMouse(Ui& ui, const Rect& area) {
    auto& g = ui.g;
    int item = 0;
    float y = area.y;
    {
        StaggerItem in(ui, stepAt, item++, stepFrom);
        g.Text(Loc::Get("keyclick_action"), { area.x, y, area.w, 24.f }, kTitle, Mono(15.f, true).Center());
        g.Text(Loc::Fmt("keyclick_will_act_as", ::SpeedrunLauncher::Input::KeyNames::Friendly(vk)), { area.x, y + 26.f, area.w, 18.f }, kDim,
               Mono(10.f).Center());
    }
    y += 26.f + 18.f + 24.f;

    // The mouse (left / right halves on top, the wheel between them)
    const float mw = 150.f, mh = 228.f;
    const Rect body{ area.CX() - mw * 0.5f, y, mw, mh };
    const float split = body.y + mh * 0.42f;
    {
        StaggerItem in(ui, stepAt, item++, 0.f, 24.f);
        g.Shadow(body.Offset(0.f, 12.f), mw * 0.5f, 22.f, Argb(0x90000000));
        g.FillRound(body, mw * 0.48f, Hex(0x0F1A26));
        const auto zone = [&](int which, const Rect& r) {
            const Id id = ui.MakeId("kc-mouse", which);
            const Interaction it = ui.Interact(id, r);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 18.f);
            const float sel = ui.Animate(HashId(id, 2), button == which ? 1.f : 0.f, 14.f);
            g.PushRoundClip(body, mw * 0.48f);
            g.FillRect(r, Palette::Teal.WithAlpha(0.08f * hover + 0.75f * sel));
            g.PopLayer();
            if (it.clicked) button = which;
            return sel;
        };
        const float wheelW = 22.f;
        zone(0, { body.x, body.y, mw * 0.5f, split - body.y });
        zone(1, { body.CX(), body.y, mw * 0.5f, split - body.y });
        // Seams
        g.FillRect({ body.CX() - 0.5f, body.y + 4.f, 1.f, split - body.y - 4.f }, Rgba(0, 0, 0, 160));
        g.FillRect({ body.x + 8.f, split, mw - 16.f, 1.f }, Rgba(0, 0, 0, 160));
        // Wheel
        const Rect wheel{ body.CX() - wheelW * 0.5f, body.y + 26.f, wheelW, 46.f };
        const Id wid = ui.MakeId("kc-mouse", 2);
        const Interaction wit = ui.Interact(wid, wheel.Inset(-6.f, -4.f));
        if (wit.hovered) ui.SetCursor(IDC_HAND);
        const float whover = ui.Animate(HashId(wid, 1), wit.hovered ? 1.f : 0.f, 18.f);
        const float wsel = ui.Animate(HashId(wid, 2), button == 2 ? 1.f : 0.f, 14.f);
        if (wsel > 0.01f) g.GlowRound(wheel, wheelW * 0.5f, Palette::Teal, 10.f, 0.8f * wsel);
        g.FillRound(wheel, wheelW * 0.5f, button == 2 ? Palette::Teal : Hex(0x1C2A38).WithAlpha(1.f));
        g.StrokeRound(wheel.Inset(0.5f), wheelW * 0.5f - 0.5f, Palette::Teal.WithAlpha(0.3f + 0.7f * std::max(whover, wsel)), 1.f);
        for (int i = 0; i < 4; ++i)
            g.FillRect({ wheel.x + 5.f, wheel.y + 10.f + i * 8.f, wheelW - 10.f, 2.f }, button == 2 ? Rgba(3, 17, 13, 120) : Rgba(255, 255, 255, 40));
        if (wit.clicked) button = 2;
        g.StrokeRound(body.Inset(0.5f), mw * 0.48f, Palette::Teal.WithAlpha(0.6f), 1.5f);
        g.GlowRound(body, mw * 0.48f, Palette::Teal, 18.f, 0.25f);

        // Labels on both sides, pointing at their button
        const auto label = [&](int which, bool leftSide, float ly) {
            const bool on = button == which;
            const std::wstring text = Loc::Get(ActionKey(which));
            const TextStyle ts = Mono(10.5f, true);
            const float tw = g.Measure(text, ts).w;
            const float lineFrom = leftSide ? body.x - 14.f : body.R() + 14.f;
            const float lineTo = leftSide ? body.x - 60.f : body.R() + 60.f;
            g.Line(lineFrom, ly, lineTo, ly, on ? Palette::Teal : Rgba(255, 255, 255, 50), on ? 2.f : 1.f);
            g.FillEllipse(lineFrom, ly, 3.f, 3.f, on ? Palette::Teal : Rgba(255, 255, 255, 80));
            const Rect tr{ leftSide ? lineTo - 10.f - tw : lineTo + 10.f, ly - 10.f, tw + 2.f, 20.f };
            g.Text(text, tr, on ? Palette::Teal : kDim, ts);
            const Id id = ui.MakeId("kc-label", which);
            const Interaction it = ui.Interact(id, tr.Inset(-6.f, -4.f));
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) button = which;
        };
        label(0, true, body.y + 50.f);
        label(1, false, body.y + 50.f);
        label(2, false, body.y + 100.f);
    }
    y = body.B() + 30.f;

    // Spam while held (+ its rate) — hidden until the secret combo unlocks it
    if (Settings::KeyClick::Current().spamUnlocked) {
        StaggerItem in(ui, stepAt, item++, 0.f, 24.f);
        const float w = 420.f;
        const float h = 14.f + 26.f + (spam ? 12.f + 18.f + 6.f + 20.f : 0.f) + 14.f;
        const Rect card{ area.CX() - w * 0.5f, y, w, h };
        g.Border(card, 10.f, Palette::Teal.WithAlpha(spam ? 0.08f : 0.03f), Palette::Teal.WithAlpha(spam ? 0.5f : 0.2f), 1.f);
        const Rect row{ card.x + 16.f, card.y + 14.f, w - 32.f, 26.f };
        g.Text(Loc::Get("keyclick_spam_label"), row, kTitle, Mono(11.f, true));
        const float tw = ToggleWidth(ui, spam);
        if (Toggle(ui, ui.MakeId("kc-spam"), RightAligned(row, tw, 26.f), spam)) spam = !spam;
        if (spam) {
            const float ly = row.B() + 12.f;
            g.Text(Loc::Fmt("keyclick_cps_label", clicksPerSecond), { row.x, ly, row.w, 18.f }, Palette::Teal, Mono(10.f, true));
            float cps = static_cast<float>(clicksPerSecond);
            if (Slider(ui, ui.MakeId("kc-cps"), { row.x, ly + 18.f + 6.f, row.w, 20.f }, cps, static_cast<float>(Settings::KeyClick::kMinCps),
                       static_cast<float>(Settings::KeyClick::kMaxCps), 1.f))
                clicksPerSecond = static_cast<int>(cps + 0.5f);
        }
    }
}

// ── Panel ───────────────────────────────────────────────────────────────────

void KeyClickWizard::Draw(Ui& ui, const Rect& screen) {
    if (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)) {
        if (step == 1 && editing < 0) SetStep(0);
        else popup.Close();
    }
    PanelStyle style;
    style.title = Loc::Get("keyclick_wizard_title");
    style.radius = 10.f;
    OverlayFrame frame(ui, ui.MakeId("keyclick-wizard"), screen, 900.f, 620.f, style, popup.Opacity(), popup.Scale());
    const Rect body = frame.body.Inset(30.f, 24.f);
    if (step == 0) DrawKeyboard(ui, body);
    else DrawMouse(ui, body);

    // Footer: ‹ change key (mouse step) · cancel · save
    const Rect footer = frame.footer.Inset(20.f, 0.f);
    if (step == 1) {
        const std::wstring change = Loc::Get("keyclick_change_key");
        const float cw = MenuKit::FooterButtonWidth(ui, change, Glyphs::Back, 120.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("kc-change"), { footer.x, footer.CY() - 18.f, cw, 36.f }, change, MenuKit::Kind::Secondary, true, Glyphs::Back))
            SetStep(0);
        const std::wstring save = Loc::Get("save_btn");
        const float sw = MenuKit::FooterButtonWidth(ui, save, Glyphs::Save, 140.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("kc-save"), { footer.R() - sw, footer.CY() - 18.f, sw, 36.f }, save, MenuKit::Kind::Primary, true, Glyphs::Save))
            Save();
        const std::wstring cancel = Loc::Get("cancel");
        const float xw = MenuKit::FooterButtonWidth(ui, cancel, Glyphs::Cancel, 110.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("kc-cancel"), { footer.R() - sw - 10.f - xw, footer.CY() - 18.f, xw, 36.f }, cancel, MenuKit::Kind::Secondary,
                                  true, Glyphs::Cancel))
            popup.Close();
    } else {
        const std::wstring cancel = Loc::Get("cancel");
        const float xw = MenuKit::FooterButtonWidth(ui, cancel, Glyphs::Cancel, 110.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("kc-cancel"), { footer.R() - xw, footer.CY() - 18.f, xw, 36.f }, cancel, MenuKit::Kind::Secondary, true,
                                  Glyphs::Cancel))
            popup.Close();
    }
    if (frame.BackdropClicked()) popup.Close();
}

} // namespace SpeedrunLauncher::Main
