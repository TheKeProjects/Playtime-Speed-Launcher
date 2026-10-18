// Hand Mods → "Individual Hands": a mod's single-colour variants as a grid of picture cards, each
// installed / removed on its own (hand-colour conflicts resolved like a full install), with the
// result shown inside the panel instead of a separate dialog.
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"
#include "Localization/Localization.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;
namespace HM = Services::HandMods;

namespace {

constexpr double kOpen = 0.34, kClose = 0.18;
constexpr float kRadius = 12.f, kHeaderH = 60.f, kFooterH = 60.f;
constexpr float kGap = 14.f, kPictureH = 150.f, kInfoH = 74.f;
constexpr Color kStageBg = Hex(0x0E2A4E);
constexpr Color kRemove = Rgba(204, 51, 51);

ImagePtr FindHand(const std::vector<HandPreview>& previews, const std::wstring& color) {
    for (const auto& p : previews)
        if (_wcsicmp(p.color.c_str(), color.c_str()) == 0) return p.image;
    return nullptr;
}

} // namespace

void HandModsOverlay::CloseExtras() {
    if (extrasPanel.open && extrasPanel.closingAt < 0.0) extrasPanel.closingAt = Async::Now();
}

// ── Install / remove ────────────────────────────────────────────────────────

void HandModsOverlay::FinishExtra(int generation, int index, bool ok, const std::wstring& message) {
    ++listRevision;   // installed flags are re-read (the mod list too)
    auto& p = extrasPanel;
    if (!p.open || p.generation != generation) return;
    p.busy = -1;
    if (ok && index >= 0 && index < static_cast<int>(p.doneAt.size())) p.doneAt[static_cast<size_t>(index)] = Async::Now();
    if (!message.empty()) {
        p.message = message;
        p.messageError = !ok;
        p.messageAt = Async::Now();
    }
}

void HandModsOverlay::UninstallExtra(int index) {
    auto& p = extrasPanel;
    if (!paks || p.busy >= 0) return;
    p.busy = index;
    const std::wstring dir = *paks, baseName = p.extras[static_cast<size_t>(index)].baseName, color = p.extras[static_cast<size_t>(index)].color;
    const int generation = p.generation;
    Async::Run([dir, baseName] {
        HM::UninstallByBaseName(dir, baseName);
        return 0;
    }, [this, generation, color](int) { FinishExtra(generation, -1, true, Loc::Fmt("hm_color_removed", color)); });
}

void HandModsOverlay::InstallExtra(int index) {
    auto& p = extrasPanel;
    if (!paks || p.busy >= 0) return;
    p.busy = index;
    const HM::Extra e = p.extras[static_cast<size_t>(index)];
    const std::wstring parent = p.zip;
    const int ch = chapter, generation = p.generation;
    // Extract the nested zip, then resolve hand-colour conflicts like a normal install
    Async::Run([parent, e, ch] { return HM::ExtractExtra(parent, e, ch); }, [this, e, generation, index](std::optional<std::wstring> extraZip) {
        if (!extraZip || !paks) {
            FinishExtra(generation, index, false, Loc::Fmt("hm_color_extract_failed", e.color));
            return;
        }
        const std::vector<std::wstring> hands{ e.color };
        std::vector<std::wstring> conflicting;
        for (const auto& [baseName, markerHands] : HM::AllInstalledHandMarkers(*paks))
            if (_wcsicmp(baseName.c_str(), e.baseName.c_str()) != 0 && HM::HandsConflict(hands, markerHands)) conflicting.push_back(baseName);
        const std::wstring dir = *paks, zipPath = *extraZip;
        const auto doInstall = [this, dir, zipPath, e, hands, conflicting, generation, index] {
            Async::Run([dir, zipPath, e, hands, conflicting] {
                for (const auto& c : conflicting) HM::UninstallByBaseName(dir, c);
                return HM::Install(dir, zipPath, e.baseName, hands);
            }, [this, e, generation, index](bool ok) {
                FinishExtra(generation, index, ok, ok ? Loc::Fmt("hm_color_installed", e.color) : Loc::Fmt("hm_color_install_failed", e.color));
            });
        };
        if (conflicting.empty()) {
            doInstall();
            return;
        }
        AskConflict(extrasPanel.mod.name + L" \x00B7 " + e.color, hands, conflicting, [this, doInstall, generation](bool proceed) {
            if (proceed) doInstall();
            else FinishExtra(generation, -1, false, L"");
        });
    });
}

// ── Panel ───────────────────────────────────────────────────────────────────

void HandModsOverlay::DrawExtras(Ui& ui, const Rect& screenRect) {
    auto& g = ui.g;
    auto& p = extrasPanel;
    const double now = Async::Now();

    float closeT = 0.f;
    if (p.closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - p.closingAt) / kClose));
        if (closeT >= 1.f) {
            const int generation = p.generation;
            p = {};
            p.generation = generation;
            return;
        }
        ui.KeepAnimating();
    }
    const bool interactive = p.closingAt < 0.0;
    if (interactive && ui.KeyPressed(VK_ESCAPE)) CloseExtras();

    // Installed flags: read from disk only after an (un)install
    const size_t n = p.extras.size();
    if (p.revision != listRevision || p.installed.size() != n) {
        p.installed.assign(n, false);
        for (size_t i = 0; i < n; ++i) p.installed[i] = paks && HM::IsInstalled(*paks, p.extras[i].baseName);
        p.revision = listRevision;
    }
    const int installedCount = static_cast<int>(std::count(p.installed.begin(), p.installed.end(), true));

    // Pop-in / pop-out
    const float sinceOpen = static_cast<float>(now - p.openedAt);
    const float openT = Anim::Clamp01(sinceOpen / static_cast<float>(kOpen));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.5f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.93f, 1.f, Anim::EaseOutBack(openT, 0.7f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);
    g.FillRect(screenRect, Palette::BackdropDark);
    const Interaction backdrop = ui.Interact(ui.MakeId("hand-extras-backdrop"), screenRect, true, false);

    // Layout: 3 columns when there's room, else 2
    const float width = std::min(screenRect.w - 60.f, 780.f);
    const int columns = width >= 700.f ? 3 : 2;
    const float innerW = width - 40.f;
    const float cardW = (innerW - kGap * (columns - 1)) / columns, cardH = kPictureH + kInfoH;
    const int rows = static_cast<int>((n + columns - 1) / columns);
    const float gridH = rows * cardH + std::max(0, rows - 1) * kGap;
    const float viewH = std::min(gridH + 8.f, std::max(cardH + 8.f, screenRect.h - 80.f - kHeaderH - kFooterH - 40.f));
    const Rect card = screenRect.Center(width, kHeaderH + 20.f + viewH + 20.f + kFooterH);

    PanelBackdropGlow(ui, card, Palette::Teal, sinceOpen);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.Block(card);
    PanelGlow(ui, card, kRadius, Palette::Teal, sinceOpen);
    g.Border(card, kRadius, Palette::PanelBg, Palette::PanelBorder, 1.f);

    // ── Header: ✋ Individual Hands / "Mod · 6 hands" ..... [2 INSTALLED] [✕] ──
    const Rect header{ card.x + 1.f, card.y + 1.f, card.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Palette::HeaderBg);
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    Badge(ui, header.x + 34.f, header.CY(), 18.f, L"\x270B", Mono(15.f).WithFont(Font::Ui).Center(), Palette::Teal);
    const float tx = header.x + 64.f;
    float right = header.R() - 16.f - 32.f;
    const ButtonStyle round{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 16.f, { 0.f }, 0.f, 32.f };
    if (TextButton(ui, ui.MakeId("hand-extras-close"), { right, header.CY() - 16.f, 32.f, 32.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, round) &&
        interactive)
        CloseExtras();
    right -= 12.f;
    if (installedCount > 0) {
        const std::wstring text = Loc::Fmt("hm_installed_count", installedCount);
        right -= PillWidth(g, text);
        Pill(g, right, header.CY(), text, kGreen, true);
        right -= 12.f;
    }
    g.Text(Loc::Get("hm_extras_title"), { tx, header.y + 12.f, right - tx, 20.f }, Palette::Teal, Mono(13.f, true));
    g.Text(p.mod.name + L"  \x00B7  " + Loc::Fmt(n == 1 ? "hm_hand_count_one" : "hm_hand_count", n), { tx, header.y + 32.f, right - tx, 16.f }, kSub,
           Mono(9.f).Ellipsis());

    // ── Grid ────────────────────────────────────────────────────────────────
    const Rect view{ card.x + 20.f, header.B() + 20.f, innerW, viewH };
    bool anyMoving = false;
    {
        ScrollArea scroll(ui, ui.MakeId("hand-extras-scroll"), view, false, 60.f, ScrollArea::Bar::Thin);
        const float offset = scroll.Offset();
        for (size_t i = 0; i < n; ++i) {
            const int index = static_cast<int>(i);
            const HM::Extra& extra = p.extras[i];
            const int col = index % columns, row = index / columns;
            const Rect slot{ view.x + col * (cardW + kGap), view.y + 4.f + row * (cardH + kGap), cardW, cardH };
            if (slot.B() < view.y + offset - 20.f || slot.y > view.B() + offset + 20.f) continue;   // scrolled out
            StaggerItem in(ui, p.openedAt + 0.12, index, 0.f, 18.f, 0.05, 0.38);

            const bool isInstalled = p.installed[i];
            const bool busy = p.busy == index;
            ui.PushId(static_cast<int64_t>(index));
            const float lit = ui.Animate(ui.MakeId("lit"), isInstalled ? 1.f : 0.f, 8.f, isInstalled ? 1.f : 0.f);
            const Card c = DrawCard(ui, ui.MakeId("card"), slot, kGreen, 10.f, lit, interactive, false);
            if (lit > 0.f && lit < 1.f) anyMoving = true;

            // Picture (zooms a little on hover), colour name over a dark fade at its bottom
            const Rect pic{ c.r.x + 1.f, c.r.y + 1.f, c.r.w - 2.f, kPictureH };
            g.PushRoundClip(c.r.Inset(1.f), 9.f);
            g.FillRect(pic, kStageBg);
            if (auto img = FindHand(p.previews, extra.color)) {
                const float zoom = 1.f + 0.06f * c.hover;
                g.PushTransform(D2D1::Matrix3x2F::Scale(zoom, zoom, D2D1::Point2F(pic.CX(), pic.CY())));
                g.DrawImage(*img, pic, busy ? 0.45f : 1.f, Stretch::UniformToFill);
                g.PopTransform();
            } else {
                g.Text(std::wstring(1, Glyphs::Image), pic, Palette::Muted, IconFont(26.f));
            }
            g.FillLinear({ pic.x, pic.B() - 56.f, pic.w, 56.f }, 0.f, { { 0.f, Rgba(5, 10, 18, 0) }, { 1.f, Rgba(5, 10, 18, 230) } }, { 0.f, 0.f },
                         { 0.f, 1.f });
            g.Text(Text::Upper(extra.color), { pic.x + 12.f, pic.B() - 30.f, pic.w - 24.f, 22.f }, Colors::White, Mono(13.f, true).Ellipsis());
            // Busy: spinner over the picture and a light sweeping across the card
            if (busy) {
                g.FillEllipse(pic.CX(), pic.CY() - 8.f, 22.f, 22.f, Rgba(5, 10, 18, 200));
                Form::Spinner(ui, pic.CX(), pic.CY() - 8.f, 11.f, Palette::Teal);
                Shimmer(g, c.r, 10.f, now, 1.1, 1.6f);
                anyMoving = true;
            }
            g.PopLayer();
            g.FillRect({ c.r.x + 1.f, pic.B(), c.r.w - 2.f, 1.f }, Palette::Divider);

            // ✓ corner badge once installed (pops in)
            if (lit > 0.01f) {
                const float pop = Anim::EaseOutBack(lit, 1.4f);
                const float bx = c.r.R() - 18.f, by = c.r.y + 18.f;
                g.FillEllipse(bx, by, 11.f * pop, 11.f * pop, kGreen);
                g.Text(std::wstring(1, Glyphs::CheckMark), { bx - 11.f, by - 11.f, 22.f, 22.f }, Palette::PanelBg.WithAlpha(lit), IconFont(9.f));
            }
            // Success flash: a ring expanding from the badge
            const float flash = static_cast<float>(now - p.doneAt[i]);
            if (flash >= 0.f && flash < 0.7f) {
                const float t = flash / 0.7f;
                g.StrokeRound(c.r.Inset(-6.f * Anim::EaseOutCubic(t)), 10.f + 6.f * t, kGreen.WithAlpha(0.7f * (1.f - t)), 2.f);
                anyMoving = true;
            }

            // Size + action button
            const Rect info{ c.r.x + 12.f, pic.B() + 1.f, c.r.w - 24.f, kInfoH - 1.f };
            g.Text(Text::FileSize(extra.size), { info.x, info.y + 8.f, info.w, 16.f }, kSub, Mono(9.f));
            if (lit > 0.01f) {
                const float pw = PillWidth(g, Loc::Get("hm_installed"));
                const bool fading = lit < 0.999f;
                if (fading) g.PushOpacity(lit);
                Pill(g, info.R() - pw, info.y + 16.f, Loc::Get("hm_installed"), kGreen, true);
                if (fading) g.PopLayer();
            }
            const Rect b{ info.x, info.B() - 10.f - 30.f, info.w, 30.f };
            const bool enabled = interactive && p.busy < 0;
            if (!paks) {
                g.Text(Loc::Get("hm_unavailable"), b, Rgba(160, 180, 200, 140), Mono(10.f).Center());
            } else if (isInstalled) {
                const ButtonStyle s{ Palette::DangerBg, Palette::DangerBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f };
                if (GlyphTextButton(ui, ui.MakeId("uninstall"), b, Glyphs::Delete, 11.f, kRemove, busy ? Loc::Get("hm_removing") : Loc::Get("hm_uninstall"),
                                    Mono(10.f, true), kRemove, s, 7.f, enabled))
                    UninstallExtra(index);
            } else {
                const ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f };
                if (c.hover > 0.01f && enabled) g.GlowRound(b, 6.f, Palette::Teal, 12.f, 0.35f * c.hover);
                if (GlyphTextButton(ui, ui.MakeId("install"), b, Glyphs::Download, 11.f, Palette::Teal, busy ? Loc::Get("hm_installing") : Loc::Get("hm_install"),
                                    Mono(10.f, true), Palette::Teal, s, 7.f, enabled))
                    InstallExtra(index);
            }
            ui.PopId();
        }
        scroll.SetContentSize(gridH + 8.f);
    }

    // ── Footer: result note (fades in, then out) / hint ......... [Close] ────
    const Rect footer{ card.x + 1.f, card.B() - 1.f - kFooterH, card.w - 2.f, kFooterH };
    g.FillRound(footer, 0.f, 0.f, kRadius - 1.f, kRadius - 1.f, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const Rect row = footer.Inset(16.f, 0.f);
    const float since = static_cast<float>(now - p.messageAt);
    const float note = p.message.empty() ? 0.f : Anim::Clamp01(since / 0.2f) * Anim::Clamp01((4.f - since) / 0.4f);
    if (note > 0.f) {
        const Color color = p.messageError ? kRemove : kGreen;
        const float rise = (1.f - Anim::EaseOutCubic(Anim::Clamp01(since / 0.3f))) * 6.f;
        g.Text(std::wstring(1, p.messageError ? Glyphs::Warning : Glyphs::CheckMark), { row.x, row.y + rise, 16.f, row.h }, color.WithAlpha(note),
               IconFont(11.f));
        g.Text(p.message, { row.x + 24.f, row.y + rise, row.w - 170.f, row.h }, color.WithAlpha(note), Mono(10.f, true).Ellipsis());
        anyMoving = true;
    } else {
        g.Text(Loc::Get("hm_extras_hint"), { row.x, row.y, row.w - 170.f, row.h }, kFaint, Mono(9.f));
    }
    const TextStyle text = Mono(11.f, true).Center();
    const float closeW = ButtonWidth(ui, Loc::Get("close"), text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("hand-extras-footer-close"), { row.R() - closeW, row.CY() - 18.f, closeW, 36.f }, Loc::Get("close"), text, Palette::Dim,
                   Styles::Footer()) &&
        interactive)
        CloseExtras();

    PanelDecor(ui, card, kRadius, Palette::Teal, sinceOpen, header.B() - 1.f, footer.y);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();
    if (anyMoving) ui.KeepAnimatingThrottled();   // spinners / flashes / the note's fade; idle otherwise
    ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(card)) CloseExtras();
}

} // namespace SpeedrunLauncher::Main
