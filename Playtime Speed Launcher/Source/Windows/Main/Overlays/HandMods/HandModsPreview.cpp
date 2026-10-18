// Hand Mods → preview viewer: one big picture per hand colour on a stage (slides between them),
// arrows and ← → keys, a strip of thumbnails that follows the selection, and Install right there.
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"
#include "Localization/Localization.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;
namespace HM = Services::HandMods;

namespace {

constexpr double kOpen = 0.34, kClose = 0.18, kSwitch = 0.36;
constexpr float kRadius = 12.f, kHeaderH = 56.f, kFooterH = 60.f;
constexpr float kThumb = 64.f, kThumbGap = 12.f, kThumbLabelH = 16.f;
constexpr Color kStageBg = Hex(0x0E2A4E);

// Round ‹ / › button beside the stage. Returns true when clicked.
bool Arrow(Ui& ui, Id id, float cx, float cy, bool left) {
    auto& g = ui.g;
    const Rect r{ cx - 20.f, cy - 20.f, 40.f, 40.f };
    const Interaction it = ui.Interact(id, r);
    const float hover = ui.Animate(HashId(1, id), it.hovered ? 1.f : 0.f, 16.f);
    const float press = ui.Animate(HashId(2, id), it.held ? 1.f : 0.f, 30.f);
    const float radius = 20.f + 2.f * hover - 2.f * press;
    if (hover > 0.01f) g.GlowRound(r, 20.f, Palette::Teal, 14.f, 0.35f * hover);
    g.FillEllipse(cx, cy, radius, radius, Lerp(Palette::ButtonBg, Palette::ConfirmBg, hover));
    g.StrokeEllipse(cx, cy, radius, radius, Lerp(Palette::ControlBorder, Palette::Teal, hover), 1.f);
    const float nudge = (left ? -3.f : 3.f) * hover;
    g.Text(left ? L"\x2039" : L"\x203A", { cx - 20.f + nudge, cy - 22.f, 40.f, 40.f }, Lerp(Palette::Hint, Palette::Teal, hover), Mono(24.f, true).Center());
    if (it.hovered) ui.SetCursor(IDC_HAND);
    return it.clicked;
}

} // namespace

void HandModsOverlay::SelectPreview(int index) {
    const int n = static_cast<int>(viewer.images.size());
    if (n == 0) return;
    const int wrapped = ((index % n) + n) % n;
    if (wrapped == viewer.selected) return;
    viewer.direction = index > viewer.selected ? 1 : -1;   // from the raw index: "next" past the end still slides forward
    viewer.previous = viewer.selected;
    viewer.selected = wrapped;
    viewer.switchedAt = Async::Now();
}

void HandModsOverlay::ClosePreview() {
    if (viewer.open && viewer.closingAt < 0.0) viewer.closingAt = Async::Now();
}

void HandModsOverlay::DrawPreview(Ui& ui, const Rect& screenRect) {
    auto& g = ui.g;
    const double now = Async::Now();

    // Pop-out, then forget the pictures
    float closeT = 0.f;
    if (viewer.closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - viewer.closingAt) / kClose));
        if (closeT >= 1.f) {
            viewer = {};
            return;
        }
        ui.KeepAnimating();
    }
    const bool interactive = viewer.closingAt < 0.0;
    const int n = static_cast<int>(viewer.images.size());
    if (interactive) {
        if (ui.KeyPressed(VK_ESCAPE)) ClosePreview();
        if (n > 1 && ui.KeyPressed(VK_RIGHT)) SelectPreview(viewer.selected + 1);
        if (n > 1 && ui.KeyPressed(VK_LEFT)) SelectPreview(viewer.selected - 1);
    }

    // Pop-in: fade + scale 0.93 → 1 with an overshoot
    const float sinceOpen = static_cast<float>(now - viewer.openedAt);
    const float openT = Anim::Clamp01(sinceOpen / static_cast<float>(kOpen));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.5f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.93f, 1.f, Anim::EaseOutBack(openT, 0.7f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);

    g.FillRect(screenRect, Palette::BackdropDark);
    const Interaction backdrop = ui.Interact(ui.MakeId("hand-viewer-backdrop"), screenRect, true, false);

    // Layout: header, stage (square), colour name, thumbnails, footer
    const float stage = std::clamp(screenRect.h - 340.f, 220.f, 460.f);
    const float width = std::min(screenRect.w - 60.f, std::max(stage + 220.f, 680.f));
    const float stripH = kThumb + 6.f + kThumbLabelH + 8.f;
    const float height = kHeaderH + 20.f + stage + 54.f + stripH + kFooterH;
    const Rect card = screenRect.Center(width, height);

    PanelBackdropGlow(ui, card, Palette::Teal, sinceOpen);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.Block(card);
    PanelGlow(ui, card, kRadius, Palette::Teal, sinceOpen);
    g.Border(card, kRadius, Palette::PanelBg, Palette::PanelBorder, 1.f);

    // ── Header: ✋ name / "6 hands · 12 MB" ......... [✕] ─────────────────────
    const Rect header{ card.x + 1.f, card.y + 1.f, card.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Palette::HeaderBg);
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    Badge(ui, header.x + 32.f, header.CY(), 17.f, L"\x270B", Mono(15.f).WithFont(Font::Ui).Center(), Palette::Teal);
    const float tx = header.x + 60.f;
    g.Text(viewer.mod.name, { tx, header.y + 10.f, header.w - 140.f, 20.f }, kText, Mono(13.f, true).Ellipsis());
    g.Text(Loc::Fmt(n == 1 ? "hm_hand_count_one_caps" : "hm_hand_count_caps", n) + L"  \x00B7  " + Text::FileSize(viewer.mod.size),
           { tx, header.y + 30.f, header.w - 140.f, 16.f }, kSub, Mono(9.f, true));
    const ButtonStyle round{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 16.f, { 0.f }, 0.f, 32.f };
    if (TextButton(ui, ui.MakeId("hand-viewer-close"), { header.R() - 16.f - 32.f, header.CY() - 16.f, 32.f, 32.f }, L"\x2715",
                   Mono(12.f, true).WithFont(Font::Ui).Center(), Palette::Dim, round) &&
        interactive)
        ClosePreview();

    // ── Stage ───────────────────────────────────────────────────────────────
    const Rect stageR{ card.CX() - stage * 0.5f, header.B() + 20.f, stage, stage };
    g.FillRadial(stageR.Inset(-70.f), { { 0.f, Palette::Teal.WithAlpha(0.10f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } });
    g.FillRound(stageR, kRadius, kStageBg);
    const float sw = Anim::Clamp01(static_cast<float>((now - viewer.switchedAt) / kSwitch));
    const float swEase = Anim::EaseOutCubic(sw);
    if (sw < 1.f) ui.KeepAnimating();
    if (n == 0) {
        g.Text(std::wstring(1, Glyphs::Image), stageR.Offset(0.f, -16.f), Palette::Muted, IconFont(34.f));
        g.Text(Loc::Get("hm_no_preview"), stageR.Offset(0.f, 30.f), kSub, Mono(11.f, true).Center());
    } else {
        // Hovering the stage zooms the picture in a little
        const bool overStage = interactive && ui.MouseOver(stageR);
        const float zoom = 1.f + 0.05f * ui.Animate(ui.MakeId("hand-viewer-zoom"), overStage ? 1.f : 0.f, 8.f);
        const float shift = stage * 0.22f * static_cast<float>(viewer.direction);
        g.PushRoundClip(stageR, kRadius);
        g.PushTransform(D2D1::Matrix3x2F::Scale(zoom, zoom, D2D1::Point2F(stageR.CX(), stageR.CY())));
        if (sw < 1.f && viewer.previous >= 0 && viewer.previous < n && viewer.images[static_cast<size_t>(viewer.previous)].image)
            g.DrawImage(*viewer.images[static_cast<size_t>(viewer.previous)].image, stageR.Offset(-shift * swEase, 0.f), 1.f - swEase,
                        Stretch::UniformToFill);
        if (const auto& img = viewer.images[static_cast<size_t>(viewer.selected)].image)
            g.DrawImage(*img, stageR.Offset(shift * (1.f - swEase), 0.f), sw < 1.f ? swEase : 1.f, Stretch::UniformToFill);
        else
            g.Text(std::wstring(1, Glyphs::Image), stageR, Palette::Muted, IconFont(34.f));
        g.PopTransform();
        // Darken the bottom edge a little so the stage sits on the card
        g.FillLinear({ stageR.x, stageR.B() - 60.f, stageR.w, 60.f }, 0.f, { { 0.f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 90) } }, { 0.f, 0.f },
                     { 0.f, 1.f });
        g.PopLayer();
    }
    g.StrokeRound(stageR, kRadius, Palette::Teal.WithAlpha(0.35f), 1.f);
    if (n > 1 && interactive) {
        if (Arrow(ui, ui.MakeId("hand-viewer-prev"), stageR.x - 44.f, stageR.CY(), true)) SelectPreview(viewer.selected - 1);
        if (Arrow(ui, ui.MakeId("hand-viewer-next"), stageR.R() + 44.f, stageR.CY(), false)) SelectPreview(viewer.selected + 1);
    }

    // Colour name drops in on every switch, with "2 / 6" under it
    if (n > 0) {
        const float rise = (1.f - swEase) * 8.f;
        const Rect nameR{ card.x, stageR.B() + 12.f - rise, card.w, 22.f };
        const std::wstring& color = viewer.images[static_cast<size_t>(viewer.selected)].color;
        g.Text(Text::Upper(color), nameR, Palette::Teal.WithAlpha(sw < 1.f ? swEase : 1.f), Mono(15.f, true).Center());
        g.Text(std::to_wstring(viewer.selected + 1) + L" / " + std::to_wstring(n), { card.x, stageR.B() + 34.f, card.w, 14.f }, kFaint,
               Mono(9.f, true).Center());
    }

    // ── Thumbnails (scroll to keep the selected one in view) ────────────────
    const Rect strip{ card.x + 20.f, stageR.B() + 54.f, card.w - 40.f, stripH };
    if (n > 0) {
        const float total = n * kThumb + (n - 1) * kThumbGap;
        float startX = strip.CX() - total * 0.5f;
        if (total > strip.w) {
            const float target = std::clamp(viewer.selected * (kThumb + kThumbGap) + kThumb * 0.5f - strip.w * 0.5f, 0.f, total - strip.w);
            startX = strip.x - ui.Animate(ui.MakeId("hand-viewer-strip"), target, 10.f, target);
        }
        g.PushClip(strip.Inset(-8.f, -8.f).Top(stripH + 8.f));
        for (int i = 0; i < n; ++i) {
            const float x = startX + i * (kThumb + kThumbGap);
            if (x + kThumb < strip.x - 8.f || x > strip.R() + 8.f) continue;   // scrolled out
            StaggerItem in(ui, viewer.openedAt + 0.12, i, 0.f, 14.f, 0.04, 0.34);
            const Rect slot{ x, strip.y + 6.f, kThumb, kThumb };
            const Interaction it = ui.Interact(ui.MakeId("hand-viewer-thumb", i), slot);
            const float hover = ui.Animate(ui.MakeId("hand-viewer-thumb-hover", i), interactive && it.hovered ? 1.f : 0.f, 16.f);
            const float sel = ui.Animate(ui.MakeId("hand-viewer-thumb-sel", i), i == viewer.selected ? 1.f : 0.f, 12.f);
            const Rect r = slot.Offset(0.f, -4.f * sel - 2.f * hover * (1.f - sel));
            if (sel > 0.01f) g.GlowRound(slot, 8.f, Palette::Teal, 12.f, 0.45f * sel);
            g.FillRound(r, 8.f, kStageBg);
            if (const auto& img = viewer.images[static_cast<size_t>(i)].image)
                g.DrawImageRounded(*img, r, 8.f, 0.5f + 0.5f * std::max(sel, hover));
            g.StrokeRound(r, 8.f, Lerp(Lerp(Palette::ButtonBorder, Palette::Hint, hover), Palette::Teal, sel), 1.f + sel);
            g.Text(viewer.images[static_cast<size_t>(i)].color, { slot.x - 8.f, slot.B() + 6.f, kThumb + 16.f, kThumbLabelH },
                   Lerp(kFaint, Palette::Teal, std::max(sel, 0.6f * hover)), Mono(8.f, true).Center().Ellipsis());
            if (!interactive) continue;
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) SelectPreview(i);
        }
        g.PopClip();
    }

    // ── Footer: key hints ........ [INSTALLED] / [Install]  [Close] ─────────
    const Rect footer{ card.x + 1.f, card.B() - 1.f - kFooterH, card.w - 2.f, kFooterH };
    g.FillRound(footer, 0.f, 0.f, kRadius - 1.f, kRadius - 1.f, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const Rect row = footer.Inset(16.f, 0.f);
    if (n > 1) g.Text(L"\x2190 \x2192  browse    Esc  close", { row.x, row.y, 260.f, row.h }, kFaint, Mono(9.f));
    else g.Text(Loc::Get("hm_esc_close"), { row.x, row.y, 260.f, row.h }, kFaint, Mono(9.f));
    const TextStyle text = Mono(11.f, true).Center();
    const float closeW = ButtonWidth(ui, Loc::Get("close"), text, Styles::Footer());
    float bx = row.R() - closeW;
    if (TextButton(ui, ui.MakeId("hand-viewer-footer-close"), { bx, row.CY() - 18.f, closeW, 36.f }, Loc::Get("close"), text, Palette::Dim, Styles::Footer()) &&
        interactive)
        ClosePreview();
    // Installed already? Install straight from here otherwise
    bool installedNow = false;
    if (mods) {
        const auto& flags = InstalledFlags();
        for (size_t i = 0; i < mods->size(); ++i)
            if ((*mods)[i].baseName == viewer.mod.baseName) installedNow = flags[i];
    }
    if (installedNow) {
        const float w = PillWidth(g, Loc::Get("hm_installed"));
        Pill(g, bx - 12.f - w, row.CY(), Loc::Get("hm_installed"), kGreen, true);
    } else if (paks) {
        const ButtonStyle s = Styles::Confirm(120.f);
        const float w = ButtonWidth(ui, Loc::Get("hm_install"), text, s);
        const Rect b{ bx - 10.f - w, row.CY() - 18.f, w, 36.f };
        g.GlowRound(b, 3.f, Palette::Teal, 14.f, 0.25f);
        if (TextButton(ui, ui.MakeId("hand-viewer-install"), b, Loc::Get("hm_install"), text, Palette::Teal, s) && interactive) {
            const HM::Mod mod = viewer.mod;
            ClosePreview();
            StartInstall(mod);
        }
    }

    PanelDecor(ui, card, kRadius, Palette::Teal, sinceOpen, header.B() - 1.f, footer.y);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();
    ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(card)) ClosePreview();
}

} // namespace SpeedrunLauncher::Main
