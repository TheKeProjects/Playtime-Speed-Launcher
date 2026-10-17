#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include "Graphics/Animation.h"
#include "UI/Style.h"
#include "UI/Ui.h"

// Pieces shared by the Hand Mods screens: hover-animated cards, pills, chevrons and the loading
// shimmer. Colors stay in the classic palette so the seasonal filter re-tints them.
namespace SpeedrunLauncher::Main::HandKit {

inline constexpr Color kCardBg = Rgba(255, 255, 255, 10);
inline constexpr Color kCardHover = Rgba(255, 255, 255, 22);
inline constexpr Color kCardBorder = Hex(0x0D2535);
inline constexpr Color kText = Hex(0xD2DCE6);
inline constexpr Color kSub = Hex(0x5A8AA0);
inline constexpr Color kFaint = Hex(0x2A5A7A);
inline constexpr Color kGreen = Hex(0x00CC88);

// 1 for `hold` seconds after something happened, then fades to 0 over a second: decorative loops
// (pulses, shimmers, waves) settle down instead of keeping the window redrawing forever.
inline float Calm(double since, double hold = 4.0) { return std::clamp(1.f - static_cast<float>(since - hold), 0.f, 1.f); }

// A clickable card: lifts and lights up its border while hovered, sinks a little while pressed.
struct Card {
    UI::Interaction it;
    float hover = 0.f;   // 0..1 (animated)
    float press = 0.f;
    Rect r;              // the card as drawn (lifted / pressed)
};

// `lit` (0..1) tints it with the accent (installed / featured). Not `interactive`: only follows the
// mouse (the card holds its own buttons, which get the clicks).
inline Card DrawCard(UI::Ui& ui, UI::Id id, const Rect& slot, Color accent, float radius = 8.f, float lit = 0.f, bool enabled = true,
                     bool interactive = true) {
    Card c;
    if (interactive) c.it = ui.Interact(id, slot);
    else c.it.hovered = ui.MouseOver(slot);
    const bool hovered = enabled && c.it.hovered;
    c.hover = ui.Animate(UI::HashId(1, id), hovered ? 1.f : 0.f, 16.f);
    c.press = ui.Animate(UI::HashId(2, id), enabled && c.it.held ? 1.f : 0.f, 30.f);
    c.r = slot.Offset(0.f, -2.f * c.hover + 1.f * c.press);
    const float glow = std::max(c.hover, 0.45f * lit);
    if (glow > 0.01f) ui.g.GlowRound(slot, radius, accent, 16.f, 0.32f * glow);   // slot: same size every frame → cached blur
    Color bg = Lerp(kCardBg, kCardHover, c.hover);
    bg = Lerp(bg, accent.WithAlpha(0.12f), 0.8f * lit);
    const Color border = Lerp(Lerp(kCardBorder, accent.WithAlpha(0.45f), lit), accent.WithAlpha(0.85f), c.hover);
    ui.g.Border(c.r, radius, bg, border, 1.f);
    if (hovered && interactive) ui.SetCursor(IDC_HAND);
    return c;
}

// Small rounded label ("INSTALLED", "AUTO", "3 MODS"). Draws at (x, cy) and returns its width.
inline float Pill(Graphics& g, float x, float cy, const std::wstring& text, Color color, bool filled = false) {
    const TextStyle style = UI::Mono(8.f, true).Center();
    const float w = g.Measure(text, style).w + 14.f;
    const Rect r{ x, cy - 9.f, w, 18.f };
    g.Border(r, 9.f, color.WithAlpha(filled ? 0.28f : 0.10f), color.WithAlpha(0.55f), 1.f);
    g.Text(text, r, color, style);
    return w;
}
inline float PillWidth(Graphics& g, const std::wstring& text) { return g.Measure(text, UI::Mono(8.f, true).Center()).w + 14.f; }

// "›" that slides right and brightens with the hover amount.
inline void Chevron(Graphics& g, const Rect& r, float hover, Color accent) {
    const float x = r.R() - 22.f + 5.f * hover;
    g.Text(L"\x203A", { x, r.y - 1.f, 14.f, r.h }, Lerp(kFaint, accent, hover), UI::Mono(18.f, true).Center());
}

// Light sweeping across `r` every `period` seconds (loading placeholders / highlighted cards).
inline void Shimmer(Graphics& g, const Rect& r, float radius, double time, double period = 1.4, float strength = 1.f) {
    const float sweep = static_cast<float>(std::fmod(time, period) / period) * 1.6f - 0.3f;
    g.PushRoundClip(r, radius);
    g.FillLinear({ r.x + r.w * sweep - r.w * 0.25f, r.y, r.w * 0.5f, r.h }, 0.f,
                 { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, static_cast<int>(16 * strength)) }, { 1.f, Rgba(255, 255, 255, 0) } },
                 { 0.f, 0.f }, { 1.f, 0.f });
    g.PopLayer();
}

// Round badge with a glyph / emoji in the middle, gently breathing.
inline void Badge(UI::Ui& ui, float cx, float cy, float radius, const std::wstring& glyph, const TextStyle& style, Color accent, float lift = 0.f) {
    auto& g = ui.g;
    const float r = radius + 1.5f * lift;
    g.FillEllipse(cx, cy, r, r, accent.WithAlpha(0.12f + 0.10f * lift));
    g.StrokeEllipse(cx, cy, r, r, accent.WithAlpha(0.45f + 0.4f * lift), 1.f);
    g.Text(glyph, { cx - r, cy - r, r * 2.f, r * 2.f }, accent, style);
}

} // namespace SpeedrunLauncher::Main::HandKit
