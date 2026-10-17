#include "UI/Widgets/Form.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"

namespace SpeedrunLauncher::UI::Form {

namespace {

struct BannerState {
    std::wstring text;
    double changedAt = -10.0;
};

struct StatusLook {
    Color color;
    wchar_t glyph;   // Segoe MDL2 Assets
};

StatusLook Look(Status kind) {
    switch (kind) {
        case Status::Success: return { Rgba(46, 204, 140), 0xE73E };   // check
        case Status::Warning: return { Rgba(255, 170, 60), 0xE7BA };   // warning
        case Status::Error: return { Rgba(235, 80, 80), 0xEA39 };      // error badge
        default: return { Rgba(120, 160, 200), 0xE946 };               // info
    }
}

TextStyle BannerText() { return Mono(10.f, true).Wrap().Top(); }

} // namespace

void Label(Ui& ui, Column& col, wchar_t glyph, const std::wstring& text, Color accent, bool required, float marginBottom) {
    auto& g = ui.g;
    const TextStyle style = Mono(9.f, true);
    const float h = std::max(14.f, g.Measure(L"Ag", style).h);
    const Rect row = col.Next(h, marginBottom);
    Glyph(ui, glyph, { row.x, row.y, 14.f, row.h }, 10.f, accent);
    // Translations already mark required fields with " *": the star glyph replaces it
    std::wstring shown = text;
    if (required) {
        while (!shown.empty() && (shown.back() == L'*' || shown.back() == L' ')) shown.pop_back();
    }
    const float tw = g.Measure(shown, style).w + 1.f;
    g.Text(shown, { row.x + 18.f, row.y, tw, row.h }, accent, style);
    if (required) g.Text(L"\x2731", { row.x + 18.f + tw + 4.f, row.y, 12.f, row.h }, accent.WithAlpha(0.8f), Mono(8.f, true));
}

bool Field(Ui& ui, Id id, const Rect& r, std::wstring& text, const TextBoxStyle& style, Color accent) {
    auto& g = ui.g;
    const float focus = ui.Animate(HashId(id, 0xF0C5), ui.Focus() == id ? 1.f : 0.f, 14.f);
    const float hover = ui.Animate(HashId(id, 0x4071), ui.MouseOver(r) ? 1.f : 0.f, 14.f);
    if (focus > 0.01f) g.GlowRound(r, style.radius, accent, 14.f, 0.5f * focus);
    TextBoxStyle s = style;
    s.border = Lerp(Lerp(style.border, accent.WithAlpha(0.45f), hover * 0.6f), accent, focus);
    s.focusBorder = s.border;
    const bool changed = TextBox(ui, id, r, text, s);
    if (style.maxLength > 0 && (focus > 0.01f || !text.empty())) {
        const std::wstring counter = std::to_wstring(text.size()) + L" / " + std::to_wstring(style.maxLength);
        const bool full = text.size() >= style.maxLength;
        g.Text(counter, { r.x, r.B() + 2.f, r.w, 12.f }, full ? Rgba(255, 170, 60) : accent.WithAlpha(0.25f + 0.35f * focus), Mono(8.f).Right());
    }
    return changed;
}

float StatusBannerHeight(Ui& ui, const std::wstring& text, float width) {
    if (text.empty()) return 0.f;
    return std::max(32.f, ui.g.Measure(text, BannerText(), width - 46.f).h + 18.f);
}

void StatusBannerAt(Ui& ui, Id id, const Rect& r, const std::wstring& text, Status kind) {
    auto& state = ui.Data<BannerState>(id);
    if (state.text != text) {
        state.text = text;
        state.changedAt = ui.Time();
    }
    if (text.empty()) return;
    auto& g = ui.g;
    const StatusLook look = Look(kind);
    const float t = Anim::Clamp01(static_cast<float>((ui.Time() - state.changedAt) / 0.35));
    const bool animating = t < 1.f;
    if (animating) {
        g.PushOpacity(Anim::EaseOutCubic(std::min(1.f, t * 1.5f)));
        ui.PushTranslate(0.f, (1.f - Anim::EaseOutBack(t, 0.6f)) * -8.f);
        ui.KeepAnimating();
    }
    g.Border(r, 6.f, look.color.WithAlpha(0.12f), look.color.WithAlpha(0.45f), 1.f);
    g.FillRound({ r.x, r.y, 3.f, r.h }, 1.5f, look.color);
    Glyph(ui, look.glyph, { r.x + 12.f, r.y, 18.f, r.h }, 13.f, look.color);
    g.Text(text, { r.x + 36.f, r.y + 9.f, r.w - 46.f, r.h - 14.f }, look.color, BannerText());
    if (animating) {
        ui.PopTransform();
        g.PopLayer();
    }
}

void StatusBanner(Ui& ui, Id id, Column& col, const std::wstring& text, Status kind) {
    const float h = StatusBannerHeight(ui, text, col.w);
    if (h <= 0.f) {
        StatusBannerAt(ui, id, {}, text, kind);   // keeps the change tracking up to date
        return;
    }
    StatusBannerAt(ui, id, col.Next(h), text, kind);
}

void Spinner(Ui& ui, float cx, float cy, float radius, Color color) {
    const double t = ui.Time();
    for (int i = 0; i < 8; ++i) {
        const float a = static_cast<float>(i) / 8.f * 6.2831853f;
        const float phase = static_cast<float>(std::fmod(t * 1.4 - i / 8.0 + 8.0, 1.0));   // 1 = head of the chase
        ui.g.FillEllipse(cx + std::cos(a) * radius, cy + std::sin(a) * radius, 1.6f + 0.8f * phase, 1.6f + 0.8f * phase,
                         color.WithAlpha(0.2f + 0.8f * phase));
    }
    ui.KeepAnimatingThrottled();
}

bool SendButton(Ui& ui, Id id, const Rect& r, const std::wstring& label, Color background, Color foreground, SendState state) {
    auto& g = ui.g;
    const float sent = ui.Animate(HashId(id, 0x5E47), state == SendState::Sent ? 1.f : 0.f, 10.f);
    const Color bg = Lerp(background, Rgba(46, 204, 140), sent);
    const float hover = ui.Animate(HashId(id, 0x4072), state == SendState::Idle && ui.MouseOver(r) ? 1.f : 0.f, 14.f);
    if (hover > 0.01f) g.GlowRound(r, 6.f, bg, 18.f, 0.5f * hover);
    ButtonScope b(ui, id, r, { bg, Colors::Transparent, 0.f, 6.f, { 0.f }, 0.f, r.h }, state == SendState::Idle);
    const TextStyle style = Mono(12.f, true);
    const float tw = g.Measure(label, style).w + 1.f;
    const float x = r.CX() - (tw + 26.f) * 0.5f;
    if (state == SendState::Sending) Spinner(ui, x + 8.f, r.CY(), 6.f, foreground);
    else Glyph(ui, state == SendState::Sent ? wchar_t(0xE73E) : wchar_t(0xE724), { x, r.y, 18.f, r.h }, 13.f, foreground);   // check / send
    g.Text(label, { x + 26.f, r.y, tw, r.h }, foreground, style);
    return b.Clicked();
}

} // namespace SpeedrunLauncher::UI::Form
