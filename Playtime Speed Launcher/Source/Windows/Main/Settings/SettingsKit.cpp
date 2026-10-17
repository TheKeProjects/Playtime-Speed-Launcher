#include "Windows/Main/Settings/SettingsKit.h"
#include <windows.h>
#include <algorithm>
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

bool SettingsContext::GlyphOption(Id id, wchar_t glyph, const std::wstring& text, float marginBottom, bool selected) {
    auto reveal = Reveal();
    const Rect r = col.Next(std::max(OptionButtonHeight(ui), ui.g.Measure(std::wstring(1, glyph), IconFont(16.f)).h + 22.f), marginBottom);
    // Selection recolors the button and its text; the glyph keeps #3A6A8A (like the old Style())
    return GlyphTextButton(ui, id, r, glyph, 16.f, Palette::Dim, text, Mono(11.f, true), selected ? Palette::Teal : Palette::Dim,
                           selected ? Styles::OptionSelected() : Styles::Option());
}

bool SettingsContext::ImageOption(Id id, const std::wstring& imageAsset, const std::wstring& text, float marginBottom) {
    auto reveal = Reveal();
    const Rect r = col.Next(std::max(OptionButtonHeight(ui), 18.f + 22.f), marginBottom);
    return ImageTextButton(ui, id, r, Images::Load(Paths::Asset(imageAsset)), 18.f, text, Mono(11.f, true), Palette::Dim, Styles::Option());
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
