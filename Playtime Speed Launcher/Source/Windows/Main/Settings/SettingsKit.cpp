#include "Windows/Main/Settings/SettingsKit.h"
#include <windows.h>
#include <algorithm>
#include <functional>
#include "App/Paths.h"
#include "Graphics/Images.h"
#include "Input/HotkeyBinding.h"
#include "Input/KeyNames.h"
#include "UI/Widgets/Button.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Controls.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

// ── Key capture ─────────────────────────────────────────────────────────────

void KeyCapture::Toggle(int t, bool allowMouse, std::function<void(const CapturedInput&)> onCaptured) {
    const bool same = target == t;
    Cancel();
    if (same) return;
    target = t;
    mouse = allowMouse;
    handler = std::move(onCaptured);
}

void KeyCapture::Poll(Ui& ui) {
    if (!target) return;
    CapturedInput result;
    bool got = false;
    for (auto& k : ui.In().keys) {
        if (!k.down || k.handled || ::SpeedrunLauncher::Input::KeyNames::IsModifier(k.vk)) continue;
        k.handled = true;
        result.vk = k.vk;
        result.escape = k.vk == VK_ESCAPE;
        if (GetKeyState(VK_CONTROL) & 0x8000) result.modifiers |= ::SpeedrunLauncher::Input::ModControl;
        if (GetKeyState(VK_SHIFT) & 0x8000) result.modifiers |= ::SpeedrunLauncher::Input::ModShift;
        if (GetKeyState(VK_MENU) & 0x8000) result.modifiers |= ::SpeedrunLauncher::Input::ModAlt;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) result.modifiers |= ::SpeedrunLauncher::Input::ModWin;
        got = true;
        break;
    }
    if (!got && mouse) {
        const auto& in = ui.In();
        if (in.pressed[static_cast<int>(MouseButton::Middle)]) result.mouseButton = ::SpeedrunLauncher::Input::MouseButtons::Middle;
        else if (in.pressed[static_cast<int>(MouseButton::X1)]) result.mouseButton = ::SpeedrunLauncher::Input::MouseButtons::XButton1;
        else if (in.pressed[static_cast<int>(MouseButton::X2)]) result.mouseButton = ::SpeedrunLauncher::Input::MouseButtons::XButton2;
        got = result.mouseButton != 0;
    }
    if (!got) return;
    auto done = std::move(handler);
    Cancel();
    if (done) done(result);
}

std::wstring BindingText(const ::SpeedrunLauncher::Input::Binding& b) {
    if (b.type == ::SpeedrunLauncher::Input::BindingType::Keyboard) return ::SpeedrunLauncher::Input::KeyNames::Friendly(b.keyVk);
    if (b.type == ::SpeedrunLauncher::Input::BindingType::Mouse) {
        switch (b.mouseButton) {
            case ::SpeedrunLauncher::Input::MouseButtons::Middle: return Loc::Get("f11_remap_mouse3");
            case ::SpeedrunLauncher::Input::MouseButtons::XButton1: return Loc::Get("f11_remap_mouse4");
            case ::SpeedrunLauncher::Input::MouseButtons::XButton2: return Loc::Get("f11_remap_mouse5");
            default: break;
        }
    }
    return Loc::Get("f11_remap_none");
}

// ── Rows ────────────────────────────────────────────────────────────────────

void SettingsContext::Section(const std::wstring& text) {
    auto reveal = Reveal();
    const float h = ui.g.Measure(text.empty() ? L"A" : text, Mono(9.f, true)).h;
    SectionLabel(ui, col.Next(h), text);
    col.Gap(10.f);
}

void SettingsContext::Rule(float above, float below) {
    auto reveal = Reveal();
    col.Gap(above);
    Divider(ui, col.x, col.y, col.w);
    col.Gap(1.f + below);
}

bool SettingsContext::ToggleRow(Id id, const std::wstring& label, bool on, float labelSize) {
    auto reveal = Reveal();
    const Rect row = col.Next(26.f);
    ui.g.Text(label, row, Palette::Label, Mono(labelSize));
    return Toggle(ui, id, RightAligned(row, ToggleWidth(ui, on), 26.f), on);
}

bool SettingsContext::KeyRow(Id id, const std::wstring& label, const std::wstring& value, bool capturing) {
    auto reveal = Reveal();
    const Rect row = col.Next(30.f);
    ui.g.Text(label, row, Palette::Label, Mono(12.f));
    const float w = ButtonWidth(ui, value, Mono(10.f), Styles::KeyBind());
    return KeyBindButton(ui, id, RightAligned(row, w, 30.f), value, capturing);
}

bool SettingsContext::Option(Id id, const std::wstring& text, bool selected, float marginBottom, float fontSize, Font font) {
    auto reveal = Reveal();
    const Rect r = col.Next(OptionButtonHeight(ui, fontSize), marginBottom);
    return OptionButton(ui, id, r, text, selected, fontSize, HAlign::Left, font);
}

namespace {

// Full-width option with an icon (glyph or picture): lights its border, lifts its icon in a small
// badge and slides a chevron in while hovered; the selected one glows teal
bool IconOption(Ui& ui, Id id, const Rect& r, const std::function<void(const Rect&, float)>& icon, const std::wstring& text, bool selected) {
    auto& g = ui.g;
    const float hover = ui.Animate(HashId(id, 0x1C09), ui.Hot(id) ? 1.f : 0.f, 16.f, 0.f);
    const float sel = ui.Animate(HashId(id, 0x1C0A), selected ? 1.f : 0.f, 14.f);
    const float lit = std::max(hover, sel);
    if (lit > 0.01f) g.GlowRound(r, 4.f, Palette::Teal, 12.f, 0.22f * lit);
    ButtonStyle style = Styles::Option();
    style.background = Lerp(Lerp(Palette::OptionBg, Hex(0x0A1A28), hover), Palette::SelectedBg, sel);
    style.border = Lerp(Palette::ButtonBorder, Palette::Teal.WithAlpha(0.8f), lit);
    ButtonScope b(ui, id, r, style);
    const float size = r.h - 14.f;
    const Rect badge{ r.x + 7.f, r.CY() - size * 0.5f, size, size };
    g.Border(badge, 4.f, Palette::Teal.WithAlpha((10.f + 20.f * lit) / 255.f), Palette::Teal.WithAlpha((40.f + 120.f * lit) / 255.f), 1.f);
    ui.PushScale(1.f + 0.1f * hover, badge.CX(), badge.CY());
    icon(badge, lit);
    ui.PopTransform();
    const Rect textArea{ badge.R() + 12.f + 3.f * hover, r.y, r.R() - 30.f - badge.R() - 12.f, r.h };
    g.Text(text, textArea, Lerp(Palette::Dim, Palette::Teal, lit), Mono(11.f, true).Ellipsis());
    Glyph(ui, Glyphs::Forward, { r.R() - 26.f + 4.f * hover, r.y, 16.f, r.h }, 9.f, Palette::Teal.WithAlpha(0.25f + 0.75f * hover));
    return b.Clicked();
}

} // namespace

bool SettingsContext::GlyphOption(Id id, wchar_t glyph, const std::wstring& text, float marginBottom, bool selected) {
    auto reveal = Reveal();
    const Rect r = col.Next(std::max(OptionButtonHeight(ui), ui.g.Measure(std::wstring(1, glyph), IconFont(16.f)).h + 22.f) + 6.f, marginBottom);
    return IconOption(ui, id, r, [&](const Rect& badge, float lit) { Glyph(ui, glyph, badge, 14.f, Lerp(Palette::Dim, Palette::Teal, lit)); }, text, selected);
}

bool SettingsContext::ImageOption(Id id, const std::wstring& imageAsset, const std::wstring& text, float marginBottom) {
    auto reveal = Reveal();
    const Rect r = col.Next(std::max(OptionButtonHeight(ui), 18.f + 22.f) + 6.f, marginBottom);
    return IconOption(ui, id, r, [&](const Rect& badge, float) {
        if (auto img = Images::Load(Paths::Asset(imageAsset))) ui.g.DrawImageRounded(*img, badge.Inset(4.f), 3.f);
    }, text, false);
}

void SettingsContext::Paragraph(const std::wstring& text, float size, Color color, float marginTop, float marginBottom, bool bold) {
    auto reveal = Reveal();
    col.Gap(marginTop);
    col.Gap(UI::Paragraph(ui, col.x, col.y, col.w, text, Mono(size, bold), color));
    col.Gap(marginBottom);
}

bool SettingsContext::BackButton(Id id, const std::wstring& text, float marginBottom) {
    auto reveal = Reveal();
    ButtonStyle s{ Palette::ButtonBg, Palette::ControlBorder, 1.f, 4.f, { 10.f, 5.f }, 0.f, 0.f };
    const TextStyle t = Mono(10.f, true);
    const float h = ui.g.Measure(text, t).h + 10.f + 2.f;
    const Rect r = col.Next(h, marginBottom);
    return TextButton(ui, id, { r.x, r.y, ButtonWidth(ui, text, t, s), h }, text, t, Palette::Label, s);
}

int SettingsContext::OptionGrid(std::string_view idBase, const std::vector<std::wstring>& texts, int selected, int columns, float marginBottom,
                                const std::vector<Font>& fonts, const std::vector<float>& sizes) {
    int clicked = -1;
    const int count = static_cast<int>(texts.size());
    for (int start = 0; start < count; start += columns) {
        float rowH = 0.f;
        for (int i = start; i < std::min(count, start + columns); ++i)
            rowH = std::max(rowH, OptionButtonHeight(ui, sizes.empty() ? 11.f : sizes[static_cast<size_t>(i)]));
        auto reveal = Reveal();   // each grid row slides in on its own
        const Rect row = col.Next(rowH);
        for (int i = start; i < std::min(count, start + columns); ++i) {
            const size_t k = static_cast<size_t>(i);
            if (OptionButton(ui, ui.MakeId(idBase, i), GridCell(row, columns, i - start, 8.f), texts[k], i == selected,
                             sizes.empty() ? 11.f : sizes[k], HAlign::Center, fonts.empty() ? Font::Mono : fonts[k]))
                clicked = i;
        }
        if (start + columns < count) col.Gap(8.f);
    }
    col.Gap(marginBottom);
    return clicked;
}

} // namespace SpeedrunLauncher::Main
