#include "UI/Widgets/Button.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"

namespace SpeedrunLauncher::UI {

ButtonScope::ButtonScope(Ui& u, Id id, const Rect& r, const ButtonStyle& style, bool enabled, bool sounds) : ui(u) {
    bounds = r;
    state = ui.Interact(id, r, enabled, sounds);
    float target = 1.f;
    if (!enabled) target = 0.4f;
    else if (state.held && state.hovered) target = 0.7f;
    else if (state.hovered) target = 0.85f;
    // Hover / press fade smoothly instead of jumping (enabled state changes stay instant)
    const float opacity = enabled ? ui.Animate(HashId(id, 0xB077), target, 22.f) : target;
    if (opacity < 0.999f) {
        ui.g.PushOpacity(opacity);
        layer = true;
    }
    // Slight press-in while held
    const float press = ui.Animate(HashId(id, 0xB5CA), state.held && state.hovered ? 0.965f : 1.f, 30.f);
    if (press < 0.999f) {
        ui.PushScale(press, r.CX(), r.CY());
        scaled = true;
    }
    ui.g.Border(r, style.radius, style.background, style.border, style.thickness);
    content = r.Inset(style.thickness).Inset(style.padding);
}

ButtonScope::~ButtonScope() {
    if (scaled) ui.PopTransform();
    if (layer) ui.g.PopLayer();
}

float ButtonWidthFor(float contentWidth, const ButtonStyle& style) {
    return std::max(style.minWidth, contentWidth + style.padding.H() + style.thickness * 2.f);
}

float ButtonWidth(Ui& ui, const std::wstring& text, const TextStyle& textStyle, const ButtonStyle& style) {
    return ButtonWidthFor(ui.g.Measure(text, textStyle).w, style);
}

bool TextButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& textStyle, Color textColor,
                const ButtonStyle& style, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    TextStyle t = textStyle;
    if (t.align == HAlign::Left && style.padding.l == 0.f && style.padding.r == 0.f) t.align = HAlign::Center;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content, textColor, t);
    return b.Clicked();
}

void Glyph(Ui& ui, wchar_t glyph, const Rect& r, float size, Color color) {
    TextStyle s = IconFont(size);
    s.valign = VAlign::Center;
    ui.g.Text(std::wstring(1, glyph), r, color, s);
}

bool GlyphTextButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, float glyphSize, Color glyphColor, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    const float gw = std::max(glyphSize + 2.f, 18.f);
    Glyph(ui, glyph, { b.content.x, b.content.y, gw, b.content.h }, glyphSize, glyphColor);
    TextStyle t = textStyle;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content.CutLeft(gw + gap), textColor, t);
    return b.Clicked();
}

bool ImageTextButton(Ui& ui, Id id, const Rect& r, const ImagePtr& image, float imageSize, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    if (image) ui.g.DrawImage(*image, b.content.Left(imageSize).CenterV(imageSize));
    TextStyle t = textStyle;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content.CutLeft(imageSize + gap), textColor, t);
    return b.Clicked();
}

float ToggleWidth(Ui& ui, bool on) {
    (void)on;   // same width both ways, so the knob has a fixed track to travel
    return std::max(ButtonWidth(ui, L"OFF", Mono(10.f, true), Styles::Toggle()), 26.f + 18.f + ui.g.Measure(L"OFF", Mono(10.f, true)).w + 10.f);
}

// ON / OFF switch: the knob slides across, the track fills with teal and glows while on
bool Toggle(Ui& ui, Id id, const Rect& r, bool on) {
    auto& g = ui.g;
    const Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float k = ui.Animate(HashId(id, 0x7061), on ? 1.f : 0.f, 16.f);
    const float hover = ui.Animate(HashId(id, 0x7062), it.hovered ? 1.f : 0.f, 18.f, 0.f);
    const float radius = r.h * 0.5f;
    if (k > 0.01f) g.GlowRound(r, radius, Palette::Teal, 12.f, (0.18f + 0.15f * hover) * k);
    g.Border(r, radius, Lerp(Palette::ButtonBg, Rgba(0, 60, 48), k), Lerp(Lerp(Palette::ControlBorder, Palette::Dim, hover), Palette::Teal, k), 1.f);
    const float knobR = radius - 5.f;
    const float kx = Anim::Lerp(r.x + 5.f + knobR, r.R() - 5.f - knobR, Anim::EaseInOutCubic(k));
    const float squash = 1.f + 0.15f * std::sin(k * 3.14159265f);   // stretches mid-travel
    g.FillEllipse(kx, r.CY(), knobR * squash, knobR, Lerp(Palette::OffText, Palette::Teal, k));
    // Label on the free side of the track, cross-fading
    const TextStyle t = Mono(10.f, true).Center();
    if (k > 0.01f) g.Text(L"ON", { r.x + 4.f, r.y, kx - knobR - r.x - 4.f, r.h }, Palette::Teal.WithAlpha(k), t);
    if (k < 0.99f) g.Text(L"OFF", { kx + knobR, r.y, r.R() - 4.f - kx - knobR, r.h }, Palette::OffText.WithAlpha(1.f - k), t);
    return it.clicked;
}

float OptionButtonHeight(Ui& ui, float fontSize) {
    return ui.g.Measure(L"Ag", Mono(fontSize, true)).h + 20.f + 2.f;
}

// List option: lights its border and nudges its text while hovered; the selected one glows and
// keeps an accent bar on its left edge
bool OptionButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool selected, float fontSize, HAlign align, Font font) {
    auto& g = ui.g;
    const float sel = ui.Animate(HashId(id, 0x0571), selected ? 1.f : 0.f, 14.f);
    const float hover = ui.Animate(HashId(id, 0x0572), ui.Hot(id) ? 1.f : 0.f, 16.f, 0.f);
    if (sel > 0.01f) g.GlowRound(r, 4.f, Palette::Teal, 12.f, 0.22f * sel);
    ButtonStyle style = Styles::Option();
    style.background = Lerp(Lerp(Palette::OptionBg, Hex(0x0A1A28), hover), Palette::SelectedBg, sel);
    style.border = Lerp(Lerp(Palette::ButtonBorder, Palette::ControlBorder, hover), Palette::Teal, sel);
    ButtonScope b(ui, id, r, style);
    if (sel > 0.01f) g.FillRound({ r.x + 1.f, r.y + 6.f, 3.f * sel, r.h - 12.f }, 1.5f, Palette::Teal);
    TextStyle t = Mono(fontSize, true);
    t.font = font;
    t.align = align;
    t.valign = VAlign::Center;
    const float nudge = align == HAlign::Left ? 3.f * std::max(hover, sel) : 0.f;
    g.Text(text, b.content.Offset(nudge, 0.f), Lerp(Lerp(Palette::Dim, Palette::Label, hover), Palette::Teal, sel), t);
    return b.Clicked();
}

// Key binding: pulses (border + glow) while waiting for a key
bool KeyBindButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool capturing) {
    ButtonStyle s = Styles::KeyBind();
    const float hover = ui.Animate(HashId(id, 0x4B42), ui.Hot(id) ? 1.f : 0.f, 16.f, 0.f);
    s.border = Lerp(s.border, Palette::Dim, hover);
    if (capturing) {
        const float pulse = Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
        ui.g.GlowRound(r, 4.f, Palette::Teal, 12.f, 0.2f + 0.3f * pulse);
        s.border = Lerp(Palette::TealDim, Palette::Teal, pulse);
        s.background = Rgba(0, 40, 30);
        ui.KeepAnimatingThrottled();
    }
    return TextButton(ui, id, r, text, Mono(10.f).Center(), capturing ? Palette::Teal : Lerp(Palette::Label, Palette::Bright, hover), s);
}

bool LinkButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& style, Color color) {
    ButtonScope b(ui, id, r, Styles::Flat());
    TextStyle t = style;
    t.underline = true;
    ui.g.Text(text, r, color, t);
    return b.Clicked();
}

} // namespace SpeedrunLauncher::UI
