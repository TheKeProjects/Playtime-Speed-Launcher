#include "UI/Widgets/MenuKit.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"

namespace SpeedrunLauncher::UI::MenuKit {

namespace {

TextStyle Wrapped(float size, bool bold = false) {
    TextStyle s = Mono(size, bold);
    s.wrap = true;
    s.valign = VAlign::Top;
    return s;
}

} // namespace

void GlyphBadge(Ui& ui, const Rect& r, wchar_t glyph, Color accent, float glyphSize) {
    auto& g = ui.g;
    const double t = ui.Time();
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(t * 2.4));
    const float radius = r.w * 0.25f;
    const float ring = static_cast<float>(std::fmod(t, 2.0) / 2.0);
    g.StrokeRound(r.Inset(-6.f * ring), radius + 6.f * ring, accent.WithAlpha(0.35f * (1.f - ring)), 1.f);
    g.GlowRound(r, radius, accent, 18.f, 0.15f + 0.2f * pulse);
    g.Border(r, radius, accent.WithAlpha(28.f / 255.f), accent.WithAlpha(130.f / 255.f), 1.f);
    // The glyph bobs gently
    Glyph(ui, glyph, r.Offset(0.f, 2.f * static_cast<float>(std::sin(t * 2.4))), glyphSize, accent);
    ui.KeepAnimatingThrottled();
}

void Hero(Ui& ui, const Rect& row, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, Color accent) {
    auto& g = ui.g;
    const Rect badge{ row.x + 2.f, row.CY() - 24.f, 48.f, 48.f };
    GlyphBadge(ui, badge, glyph, accent);
    const float tx = badge.R() + 16.f, tw = row.R() - tx;
    if (subtitle.empty()) {
        g.Text(title, { tx, row.y, tw, row.h }, kTitle, Mono(14.f, true).Ellipsis());
        return;
    }
    g.Text(title, { tx, row.CY() - 21.f, tw, 20.f }, kTitle, Mono(14.f, true).Ellipsis());
    g.Text(subtitle, { tx, row.CY() + 2.f, tw, 16.f }, kDim, Mono(10.f).Ellipsis());
}

float NoteHeight(Ui& ui, const std::wstring& text, float width) { return ui.g.Measure(text, Wrapped(10.f), width - 52.f).h + 24.f; }

void Note(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text, Color accent, bool spin) {
    auto& g = ui.g;
    g.Border(r, 6.f, accent.WithAlpha(18.f / 255.f), accent.WithAlpha(70.f / 255.f), 1.f);
    g.FillRound({ r.x + 1.f, r.y + 8.f, 2.f, r.h - 16.f }, 1.f, accent.WithAlpha(0.85f));
    const Rect icon{ r.x + 10.f, r.CY() - 12.f, 24.f, 24.f };
    if (spin) {
        g.PushTransform(D2D1::Matrix3x2F::Rotation(static_cast<float>(std::fmod(ui.Time() * 90.0, 360.0)), D2D1::Point2F(icon.CX(), icon.CY())));
        ui.KeepAnimatingThrottled();
    }
    Glyph(ui, glyph, icon, 13.f, accent);
    if (spin) g.PopTransform();
    const float th = r.h - 24.f;
    g.Text(text, { r.x + 42.f, r.CY() - th * 0.5f, r.w - 52.f, th }, Lerp(accent, Palette::Bright, 0.35f), Wrapped(10.f));
}

float ActionCardHeight(bool withSubtitle) { return withSubtitle ? 62.f : 50.f; }

bool ActionCard(Ui& ui, Id id, const Rect& slot, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, Color accent,
                bool enabled) {
    auto& g = ui.g;
    const Interaction it = ui.Interact(id, slot, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const float hover = ui.Animate(HashId(id, 0xCA7D), it.hovered && enabled ? 1.f : 0.f, 14.f, 0.f);
    const float press = ui.Animate(HashId(id, 0xCA7E), it.held && it.hovered ? 1.f : 0.f, 30.f, 0.f);
    const Rect r = slot.Offset(0.f, -2.f * hover + 1.f * press);
    if (!enabled) g.PushOpacity(0.4f);

    if (hover > 0.01f) g.GlowRound(r, 8.f, accent, 16.f, 0.35f * hover);
    g.Border(r, 8.f, Lerp(Palette::OptionBg, accent.WithAlpha(22.f / 255.f), hover), Lerp(Palette::ButtonBorder, accent.WithAlpha(0.8f), hover), 1.f);
    // Light wash from the left while hovered (gradient brushes are cached by their stops: quantized)
    const float wash = std::round(hover * 10.f) / 10.f;
    if (wash > 0.f)
        g.FillLinear(r.Inset(1.f), 7.f, { { 0.f, accent.WithAlpha(0.10f * wash) }, { 0.6f, accent.WithAlpha(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });

    const float badge = r.h - 22.f;
    const Rect b{ r.x + 11.f, r.CY() - badge * 0.5f, badge, badge };
    g.Border(b, 8.f, accent.WithAlpha((24.f + 30.f * hover) / 255.f), accent.WithAlpha((90.f + 100.f * hover) / 255.f), 1.f);
    ui.PushScale(1.f + 0.08f * hover, b.CX(), b.CY());
    Glyph(ui, glyph, b, badge * 0.42f, accent);
    ui.PopTransform();

    const float tx = b.R() + 14.f + 2.f * hover, tw = r.R() - 36.f - tx;
    if (subtitle.empty()) {
        g.Text(title, { tx, r.y, tw, r.h }, Lerp(Rgba(200, 216, 228), accent, 0.5f * hover), Mono(11.f, true).Ellipsis());
    } else {
        g.Text(title, { tx, r.CY() - 18.f, tw, 18.f }, Lerp(Rgba(200, 216, 228), accent, 0.5f * hover), Mono(11.f, true).Ellipsis());
        g.Text(subtitle, { tx, r.CY() + 1.f, tw, 16.f }, kDim, Mono(9.5f).Ellipsis());
    }
    // Chevron sliding in on hover
    Glyph(ui, Glyphs::Forward, { r.R() - 32.f + 4.f * hover, r.y, 20.f, r.h }, 10.f, accent.WithAlpha(0.35f + 0.65f * hover));
    if (!enabled) g.PopLayer();
    return it.clicked && enabled;
}

float FooterButtonWidth(Ui& ui, const std::wstring& text, wchar_t glyph, float minWidth) {
    return std::max(minWidth, ui.g.Measure(text, Mono(11.f, true)).w + 32.f + (glyph ? 22.f : 0.f));
}

bool FooterButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, Kind kind, bool enabled, wchar_t glyph, Color accent) {
    auto& g = ui.g;
    const Color tone = kind == Kind::Danger ? kRed : accent;
    const float hot = ui.Animate(HashId(id, 0xF007), ui.Hot(id) && enabled ? 1.f : 0.f, 14.f, 0.f);
    if (kind == Kind::Primary && enabled) {
        const float breathe = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
        g.GlowRound(r, 6.f, tone, 14.f, 0.18f + 0.1f * breathe + 0.3f * hot);
        ui.KeepAnimatingThrottled();
    } else if (hot > 0.01f) {
        g.GlowRound(r, 6.f, tone, 12.f, 0.25f * hot);
    }
    ButtonStyle style;
    style.radius = 6.f;
    style.height = r.h;
    if (kind == Kind::Primary) {
        style.background = Lerp(tone.WithAlpha(0.85f), tone, hot);
        style.border = Colors::Transparent;
        style.thickness = 0.f;
    } else {
        style.background = Lerp(Palette::ButtonBg, tone.WithAlpha(24.f / 255.f), hot);
        style.border = Lerp(kind == Kind::Danger ? Palette::DangerBorder : Palette::ControlBorder, tone.WithAlpha(0.8f), hot);
    }
    ButtonScope b(ui, id, r, style, enabled);
    const Color fg = kind == Kind::Primary ? Rgba(3, 17, 13) : Lerp(kind == Kind::Danger ? Palette::DangerText : Palette::Label, tone, hot);
    const TextStyle ts = Mono(11.f, true);
    const float tw = g.Measure(text, ts).w + 1.f;
    const float total = tw + (glyph ? 22.f : 0.f);
    float x = r.CX() - total * 0.5f;
    if (glyph) {
        Glyph(ui, glyph, { x, r.y, 14.f, r.h }, 12.f, fg);
        x += 22.f;
    }
    g.Text(text, { x, r.y, tw + 2.f, r.h }, fg, ts);
    return b.Clicked();
}

void GlowProgress(Ui& ui, Id id, const Rect& r, float value, Color accent) {
    auto& g = ui.g;
    const float shown = ui.Animate(id, std::clamp(value, 0.f, 1.f), 10.f, 0.f);
    const float radius = r.h * 0.5f;
    g.FillRound(r, radius, Hex(0x0D2535));
    if (shown <= 0.f) return;
    const Rect fill{ r.x, r.y, std::max(r.h, r.w * shown), r.h };
    g.GlowRound(fill, radius, accent, 10.f, 0.35f);
    g.FillLinear(fill, radius, { { 0.f, Lerp(accent, Colors::Black, 0.3f) }, { 1.f, accent } }, { 0.f, 0.f }, { 1.f, 0.f });
    const float sweep = static_cast<float>(std::fmod(ui.Time() * 0.9, 1.3)) - 0.15f;
    g.PushRoundClip(fill, radius);
    g.FillLinear({ fill.x + fill.w * sweep - 40.f, fill.y, 80.f, fill.h }, 0.f,
                 { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 70) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.PopLayer();
    ui.KeepAnimatingThrottled();
}

float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color accent) {
    const TextStyle style = Mono(9.f, true).Center();
    const float w = ui.g.Measure(text, style).w + 18.f;
    const Rect r{ x, cy - 10.f, w, 20.f };
    ui.g.Border(r, 10.f, accent.WithAlpha(22.f / 255.f), accent.WithAlpha(90.f / 255.f), 1.f);
    ui.g.Text(text, r, accent, style);
    return w;
}

} // namespace SpeedrunLauncher::UI::MenuKit
