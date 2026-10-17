// Hand Mods → "Hand Conflict": installing something that changes a hand colour another installed
// mod already changes. Shows what goes in (with its hand colours) and what will be removed first,
// then Continue / Cancel. Drawn inside the menu, over everything else.
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;

namespace {

constexpr double kOpen = 0.32, kClose = 0.18;
constexpr float kRadius = 12.f, kHeaderH = 58.f, kFooterH = 62.f, kWidth = 470.f;
constexpr float kRowH = 44.f, kLabelH = 14.f;
constexpr Color kAmber = Hex(0xE8A000);
constexpr Color kRemove = Rgba(204, 51, 51);

// Dot colour for a hand name ("Green" → green); unknown ones use the accent
Color HandColor(const std::wstring& name) {
    struct Known {
        const wchar_t* name;
        Color color;
    };
    static const Known known[] = { { L"blue", Hex(0x3B78E7) },   { L"red", Hex(0xE5484D) },    { L"green", Hex(0x3CC46A) },
                                   { L"purple", Hex(0x9B5DE5) }, { L"orange", Hex(0xF2913D) }, { L"yellow", Hex(0xF2D43D) },
                                   { L"pink", Hex(0xF27BB8) },   { L"flare", Hex(0xF06A3C) },  { L"conductive", Hex(0xE0A040) },
                                   { L"pressurized", Hex(0x9AA4AE) } };
    for (const auto& k : known)
        if (_wcsicmp(k.name, name.c_str()) == 0) return k.color;
    return Palette::Teal;
}

// "● Green" chip; returns its width
float HandChip(Graphics& g, float x, float cy, const std::wstring& hand) {
    const TextStyle style = Mono(9.f, true);
    const float w = g.Measure(hand, style).w + 30.f;
    const Color c = HandColor(hand);
    const Rect r{ x, cy - 11.f, w, 22.f };
    {
        NoColorFilter keep(g);   // real hand colours, not re-tinted by the season
        g.Border(r, 11.f, c.WithAlpha(0.14f), c.WithAlpha(0.6f), 1.f);
        g.FillEllipse(r.x + 12.f, cy, 4.f, 4.f, c);
        g.Text(hand, { r.x + 21.f, r.y, w - 24.f, r.h }, c, style);
    }
    return w;
}

} // namespace

void HandModsOverlay::AskConflict(const std::wstring& incoming, const std::vector<std::wstring>& hands, const std::vector<std::wstring>& removed,
                                  std::function<void(bool)> done) {
    if (conflict.open && conflict.done) conflict.done(false);   // a previous question nobody answered
    conflict = {};
    conflict.open = true;
    conflict.incoming = incoming;
    conflict.hands = hands;
    conflict.removed = removed;
    conflict.done = std::move(done);
    conflict.openedAt = Async::Now();
}

void HandModsOverlay::AnswerConflict(bool proceed) {
    if (!conflict.open || conflict.closingAt >= 0.0) return;
    conflict.answer = proceed;
    conflict.closingAt = Async::Now();
}

void HandModsOverlay::DrawConflict(Ui& ui, const Rect& screenRect) {
    auto& g = ui.g;
    auto& c = conflict;
    const double now = Async::Now();

    // Pop-out, then hand over the answer
    float closeT = 0.f;
    if (c.closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - c.closingAt) / kClose));
        if (closeT >= 1.f) {
            auto done = std::move(c.done);
            const bool answer = c.answer;
            c = {};
            if (done) done(answer);
            return;
        }
        ui.KeepAnimating();
    }
    const bool interactive = c.closingAt < 0.0;
    if (interactive) {
        if (ui.KeyPressed(VK_ESCAPE)) AnswerConflict(false);
        if (ui.KeyPressed(VK_RETURN)) AnswerConflict(true);
    }

    const float since = static_cast<float>(now - c.openedAt);
    const float openT = Anim::Clamp01(since / static_cast<float>(kOpen));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.5f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.9f, 1.f, Anim::EaseOutBack(openT, 0.9f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);
    g.FillRect(screenRect, Palette::BackdropDark);
    const Interaction backdrop = ui.Interact(ui.MakeId("hand-conflict-backdrop"), screenRect, true, false);

    const size_t removedN = c.removed.size();
    const float bodyH = 18.f + kLabelH + 6.f + kRowH + 10.f + 26.f + 10.f + kLabelH + 6.f + removedN * kRowH + (removedN ? (removedN - 1) * 6.f : 0.f) + 18.f;
    const Rect card = screenRect.Center(kWidth, kHeaderH + bodyH + kFooterH);

    // A short shake as it lands: something needs attention
    const float shake = std::sin(since * 38.f) * 4.f * std::max(0.f, 1.f - std::fabs(since - 0.28f) / 0.18f) * (since > 0.1f ? 1.f : 0.f);
    PanelBackdropGlow(ui, card, kAmber, since);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.PushTranslate(shake, 0.f);
    ui.Block(card);
    PanelGlow(ui, card, kRadius, kAmber, since);
    g.Border(card, kRadius, Palette::PanelBg, kAmber.WithAlpha(0.35f), 1.f);

    // ── Header: ⚠ HAND CONFLICT ─────────────────────────────────────────────
    const Rect header{ card.x + 1.f, card.y + 1.f, card.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Palette::HeaderBg);
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    const float bx = header.x + 34.f, by = header.CY();
    {
        // Warning badge with a ring pulsing out of it for the first seconds
        const float calm = Calm(since, 3.0);
        if (calm > 0.f) {
            const float pulse = static_cast<float>(std::fmod(since, 1.2) / 1.2);
            g.StrokeEllipse(bx, by, 18.f + 10.f * pulse, 18.f + 10.f * pulse, kAmber.WithAlpha(0.5f * (1.f - pulse) * calm), 1.5f);
            ui.KeepAnimatingThrottled();
        }
        NoColorFilter keep(g);
        Badge(ui, bx, by, 18.f, std::wstring(1, Glyphs::Warning), IconFont(14.f), kAmber);
    }
    {
        NoColorFilter keep(g);
        g.Text(L"HAND CONFLICT", { header.x + 64.f, header.y + 12.f, header.w - 80.f, 20.f }, kAmber, Mono(13.f, true));
    }
    g.Text(c.hands.size() == 1 ? L"Only one mod can change each hand" : L"Only one mod can change each of these hands",
           { header.x + 64.f, header.y + 32.f, header.w - 80.f, 16.f }, kSub, Mono(9.f));

    // ── Body ────────────────────────────────────────────────────────────────
    float y = header.B() + 18.f;
    const float x = card.x + 20.f, w = card.w - 40.f;
    {
        // What goes in: ✋ name + its hand colours
        StaggerItem in(ui, c.openedAt + 0.08, 0, -16.f, 0.f, 0.0, 0.34);
        g.Text(L"INSTALLING", { x, y, w, kLabelH }, kGreen, Mono(8.f, true));
        const Rect r{ x, y + kLabelH + 6.f, w, kRowH };
        g.Border(r, 8.f, kGreen.WithAlpha(0.08f), kGreen.WithAlpha(0.45f), 1.f);
        g.Text(L"\x270B", { r.x + 8.f, r.y, 28.f, r.h }, kGreen, Mono(14.f).WithFont(Font::Ui).Center());
        float chipsW = 0.f;
        for (const auto& h : c.hands) chipsW += g.Measure(h, Mono(9.f, true)).w + 30.f + 6.f;
        float cx = r.R() - 10.f - chipsW + 6.f;
        g.Text(c.incoming, { r.x + 40.f, r.y, std::max(0.f, cx - r.x - 48.f), r.h }, kText, Mono(11.f, true).Ellipsis());
        for (const auto& h : c.hands) cx += HandChip(g, cx, r.CY(), h) + 6.f;
    }
    y += kLabelH + 6.f + kRowH + 10.f;
    {
        // Arrow between them, bobbing gently while the question is open
        StaggerItem in(ui, c.openedAt + 0.14, 0, 0.f, -8.f, 0.0, 0.3);
        const float calm = Calm(since, 4.0);
        const float bob = std::sin(since * 4.f) * 3.f * calm;
        g.FillRect({ card.x + 40.f, y + 13.f, card.w * 0.5f - 64.f, 1.f }, Palette::Divider);
        g.FillRect({ card.CX() + 24.f, y + 13.f, card.w * 0.5f - 64.f, 1.f }, Palette::Divider);
        g.Text(L"\x2191\x2193", { card.CX() - 24.f, y + bob, 48.f, 26.f }, kAmber, Mono(13.f, true).Center());
    }
    y += 26.f + 10.f;
    g.Text(removedN == 1 ? L"WILL BE REMOVED FIRST" : L"THESE WILL BE REMOVED FIRST", { x, y, w, kLabelH }, kRemove, Mono(8.f, true));
    y += kLabelH + 6.f;
    for (size_t i = 0; i < removedN; ++i) {
        // What goes out: crossed out one after another
        StaggerItem in(ui, c.openedAt + 0.2, static_cast<int>(i), 16.f, 0.f, 0.06, 0.34);
        const Rect r{ x, y, w, kRowH };
        g.Border(r, 8.f, kRemove.WithAlpha(0.07f), kRemove.WithAlpha(0.4f), 1.f);
        g.Text(std::wstring(1, Glyphs::Delete), { r.x + 8.f, r.y, 28.f, r.h }, kRemove, IconFont(12.f));
        const TextStyle style = Mono(11.f, true).Ellipsis();
        const float tw = std::min(g.Measure(c.removed[i], style).w, r.w - 60.f);
        g.Text(c.removed[i], { r.x + 40.f, r.y, r.w - 52.f, r.h }, Lerp(kText, kSub, 0.4f), style);
        const float strike = Anim::EaseInOutCubic(Anim::Clamp01((since - 0.45f - 0.08f * static_cast<float>(i)) / 0.35f));
        if (strike > 0.f) g.FillRect({ r.x + 38.f, r.CY(), (tw + 4.f) * strike, 1.5f }, kRemove.WithAlpha(0.85f));
        if (strike < 1.f) ui.KeepAnimating();
        y += kRowH + 6.f;
    }

    // ── Footer: Enter / Esc ........ [Cancel] [Continue] ────────────────────
    const Rect footer{ card.x + 1.f, card.B() - 1.f - kFooterH, card.w - 2.f, kFooterH };
    g.FillRound(footer, 0.f, 0.f, kRadius - 1.f, kRadius - 1.f, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const Rect row = footer.Inset(16.f, 0.f);
    g.Text(L"Enter  continue    Esc  cancel", { row.x, row.y, 200.f, row.h }, kFaint, Mono(9.f));
    const TextStyle text = Mono(11.f, true).Center();
    const ButtonStyle go = Styles::Confirm(120.f);
    const float goW = ButtonWidth(ui, L"Continue", text, go);
    const Rect goR{ row.R() - goW, row.CY() - 18.f, goW, 36.f };
    g.GlowRound(goR, 3.f, Palette::Teal, 14.f, 0.22f);
    if (TextButton(ui, ui.MakeId("hand-conflict-continue"), goR, L"Continue", text, Palette::Teal, go) && interactive) AnswerConflict(true);
    const float cancelW = ButtonWidth(ui, L"Cancel", text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("hand-conflict-cancel"), { goR.x - 10.f - cancelW, row.CY() - 18.f, cancelW, 36.f }, L"Cancel", text, Palette::Dim,
                   Styles::Footer()) &&
        interactive)
        AnswerConflict(false);

    PanelDecor(ui, card, kRadius, kAmber, since, header.B() - 1.f, footer.y);
    if (PanelEntranceRunning(since)) ui.KeepAnimating();
    ui.PopTransform();
    ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(card)) AnswerConflict(false);
}

} // namespace SpeedrunLauncher::Main
