#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/GameFiles/ModFiles.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;

namespace {

constexpr double kSlide = 0.34;     // screen change
constexpr float kStepsH = 46.f;     // CHAPTER → VERSION → MOD strip

std::wstring ChapterName(int n) { return Loc::Get("ch" + std::to_string(n) + "_title"); }

} // namespace

void HandModsOverlay::GoTo(Screen next, int direction) {
    if (next == Screen::Versions) RefreshVersions();
    screen = next;
    screenDirection = direction;
    screenChangedAt = Async::Now();
}

void HandModsOverlay::OpenHub() {
    GoTo(Screen::Hub);
    popup.Open();
}

void HandModsOverlay::OpenChapter(int number) {
    chapter = number;
    win64.reset();
    paks.reset();
    mods.reset();
    GoTo(Screen::Versions);
    popup.Open();
}

void HandModsOverlay::ShowMessage(const std::wstring& message) {
    Dialogs::Options o;
    o.title = L"Hand Mods";
    o.close = L"OK";
    Dialogs::ShowText(main.Hwnd(), o, message, Rgba(160, 180, 200, 200));
}

// ── Steps: CHAPTER → VERSION → MOD ──────────────────────────────────────────

void HandModsOverlay::DrawSteps(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const int active = screen == Screen::Hub ? 0 : screen == Screen::Versions ? 1 : 2;
    const std::wstring labels[] = { chapter > 0 && active > 0 ? Loc::Fmt("hm_step_chapter_n", chapter) : Loc::Get("hm_step_chapter"), Loc::Get("hm_step_version"),
                                    screen == Screen::Installing ? Loc::Get("hm_step_install") : Loc::Get("hm_step_mod") };
    g.FillRect({ r.x, r.B() - 1.f, r.w, 1.f }, Palette::Divider);

    const float x0 = r.x + 70.f, x1 = r.R() - 70.f, cy = r.y + 16.f;
    // The track fills up to the active step (animated)
    const float fill = ui.Animate(ui.MakeId("hand-steps-fill"), static_cast<float>(active) / 2.f, 9.f, 0.f);
    g.FillRound({ x0, cy - 1.f, x1 - x0, 2.f }, 1.f, Palette::ButtonBorder);
    if (fill > 0.f) {
        g.FillLinear({ x0, cy - 1.f, (x1 - x0) * fill, 2.f }, 1.f, { { 0.f, Palette::ConfirmBorder }, { 1.f, Palette::Teal } }, { 0.f, 0.f }, { 1.f, 0.f });
        // Spark travelling at the head of the fill while it moves
        if (std::fabs(fill * 2.f - active) > 0.01f) g.FillEllipse(x0 + (x1 - x0) * fill, cy, 4.f, 4.f, Palette::Teal.WithAlpha(0.6f));
    }
    const double now = Async::Now();
    // The pulse around the current step settles a few seconds after arriving
    const float calm = Calm(now - screenChangedAt, 4.0);
    if (calm > 0.f) ui.KeepAnimatingThrottled();
    for (int i = 0; i < 3; ++i) {
        const float x = x0 + (x1 - x0) * (i / 2.f);
        const float lit = ui.Animate(ui.MakeId("hand-step-lit", i), i <= active ? 1.f : 0.f, 10.f, 0.f);
        const bool current = i == active;
        if (current) {
            // Ring pulsing outwards from the current step
            const float pulse = static_cast<float>(std::fmod(now, 1.6) / 1.6);
            if (calm > 0.f) g.StrokeEllipse(x, cy, 6.f + 8.f * pulse, 6.f + 8.f * pulse, Palette::Teal.WithAlpha(0.45f * (1.f - pulse) * calm), 1.f);
        }
        g.FillEllipse(x, cy, 6.f, 6.f, Lerp(Palette::ButtonBg, i < active ? Palette::Teal : Palette::ConfirmBg, lit));
        g.StrokeEllipse(x, cy, 6.f, 6.f, Lerp(Palette::ControlBorder, Palette::Teal, lit), 1.f);
        if (i < active) g.Text(std::wstring(1, Glyphs::CheckMark), { x - 6.f, cy - 6.f, 12.f, 12.f }, Palette::PanelBg, IconFont(7.f));
        g.Text(labels[i], { x - 60.f, cy + 9.f, 120.f, 14.f }, Lerp(kFaint, current ? Palette::Teal : kSub, lit), Mono(8.f, true).Center());
    }
}

// ── Hub ─────────────────────────────────────────────────────────────────────

void HandModsOverlay::DrawHub(Ui& ui, Column& col) {
    auto& g = ui.g;
    const double now = Async::Now();
    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;
    {
        StaggerItem in(ui, screenChangedAt, 0, 0.f, 8.f, 0.0, 0.3);
        g.Text(Loc::Get("handmods_hub_label"), col.Next(labelH, 10.f), kSub, Mono(10.f, true));
    }
    for (int n = 1; n <= 5; ++n) {
        StaggerItem in(ui, screenChangedAt + 0.04, n, 26.f * screenDirection, 0.f, 0.045, 0.38);
        const Rect slot = col.Next(58.f, 8.f);
        const Card c = DrawCard(ui, ui.MakeId("hand-chapter", n), slot, Palette::Teal);
        // Big chapter number with a glow that follows the hover
        const Rect num{ c.r.x + 14.f, c.r.y, 44.f, c.r.h };
        const std::wstring digits = L"0" + std::to_wstring(n);
        if (c.hover > 0.01f) g.GlowText(digits, num, Mono(22.f, true).Center(), Palette::Teal, 10.f, 0.6f * c.hover);
        g.Text(digits, num, Lerp(Palette::Muted, Palette::Teal, 0.55f + 0.45f * c.hover), Mono(22.f, true).Center());
        g.FillRect({ num.R() + 8.f, c.r.y + 14.f, 1.f, c.r.h - 28.f }, Palette::ButtonBorder);

        const float tx = num.R() + 20.f + 3.f * c.hover;
        float right = c.r.R() - 30.f;
        if (auto it = cache.find(n); it != cache.end()) {
            const std::wstring count = Loc::Fmt(it->second.size() == 1 ? "hm_mod_count_one" : "hm_mod_count", it->second.size());
            right -= PillWidth(g, count);
            Pill(g, right, c.r.CY(), count, Palette::Teal);
            right -= 8.f;
        }
        g.Text(Loc::Get("chapter" + std::to_string(n) + "_section"), { tx, c.r.y + 12.f, right - tx, 18.f }, kText, Mono(11.f, true));
        g.Text(ChapterName(n), { tx, c.r.y + 30.f, right - tx, 16.f }, kSub, Mono(9.f).Ellipsis());
        Chevron(g, c.r, c.hover, Palette::Teal);
        if (c.it.clicked) OpenChapter(n);
    }

    // Submit a mod: green card with a "+" that turns on hover and a light passing over it now and then
    col.Gap(8.f);
    StaggerItem in(ui, screenChangedAt + 0.04, 6, 0.f, 14.f, 0.045, 0.4);
    const Rect slot = col.Next(64.f);
    const Card c = DrawCard(ui, ui.MakeId("hand-submit-entry"), slot, kGreen, 8.f, 1.f);
    // A light passes over it a couple of times after the hub opens, and while it's hovered
    if (Calm(now - screenChangedAt, 5.4) > 0.f || c.hover > 0.01f) {
        Shimmer(g, c.r, 8.f, now - screenChangedAt, 2.7, 1.5f);
        ui.KeepAnimatingThrottled();
    }
    const float bx = c.r.x + 34.f, by = c.r.CY();
    g.FillEllipse(bx, by, 17.f + 1.5f * c.hover, 17.f + 1.5f * c.hover, kGreen.WithAlpha(0.14f + 0.12f * c.hover));
    g.StrokeEllipse(bx, by, 17.f + 1.5f * c.hover, 17.f + 1.5f * c.hover, kGreen.WithAlpha(0.6f), 1.f);
    g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * Anim::EaseOutBack(c.hover, 0.8f), D2D1::Point2F(bx, by)));
    g.Text(L"+", { bx - 17.f, by - 18.f, 34.f, 34.f }, kGreen, Mono(20.f, true).Center());
    g.PopTransform();
    std::wstring text = Loc::Get("handmods_submit_entry_btn");
    if (!text.empty() && text[0] == L'\x2795') text = text.substr(text.size() > 1 && text[1] == L' ' ? 2 : 1);   // the badge already is the "+"
    const float tx = bx + 30.f;
    g.Text(text, { tx, c.r.y + 14.f, c.r.R() - tx - 30.f, 18.f }, kGreen, Mono(11.f, true));
    g.Text(Loc::Get("hm_submit_card_sub"), { tx, c.r.y + 33.f, c.r.R() - tx - 30.f, 16.f }, kSub,
           Mono(9.f).Ellipsis());
    Chevron(g, c.r, c.hover, kGreen);
    if (c.it.clicked) OpenSubmit();
}

// ── Version picker ──────────────────────────────────────────────────────────

void HandModsOverlay::SelectVersion(const std::wstring& exePath) {
    const auto found = App::Launcher::FindWin64Dir(Files::DirectoryName(exePath));
    if (!found) {
        ShowMessage(Loc::Get("hm_no_paks"));
        return;
    }
    win64 = found;
    paks = Services::ModFiles::PaksDir(*found);
    ++listRevision;   // another Paks folder: installed flags are re-read
    GoTo(Screen::Mods);
    LoadMods();
}

void HandModsOverlay::RefreshVersions() {
    versionRows.clear();
    auto& l = App::L();
    const auto* ch = l.ChapterByNumber(chapter);
    if (!ch) return;
    const auto autoExe = l.epic.IsEnabled() ? l.epic.ExePath(chapter) : ch->gameExePath;
    if (autoExe && !autoExe->empty() && Files::Exists(*autoExe)) versionRows.push_back({ Loc::Get("auto_name"), *autoExe, true });
    for (const auto& custom : l.store.Customs(chapter))
        if (Files::Exists(custom.exePath)) versionRows.push_back({ custom.name, custom.exePath, false });
}

void HandModsOverlay::DrawVersions(Ui& ui, Column& col) {
    auto& g = ui.g;
    const std::vector<VersionRow>& rows = versionRows;

    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;
    {
        StaggerItem in(ui, screenChangedAt, 0, 0.f, 8.f, 0.0, 0.3);
        g.Text(Loc::Get("hm_which_version") + L"  \x00B7  " + ChapterName(chapter), col.Next(labelH, 10.f), kSub, Mono(10.f, true).Ellipsis());
    }
    if (rows.empty()) {
        StaggerItem in(ui, screenChangedAt + 0.06, 0, 0.f, 12.f, 0.0, 0.4);
        const Rect r = col.Next(120.f);
        Badge(ui, r.CX(), r.y + 34.f, 22.f, std::wstring(1, Glyphs::Warning), IconFont(16.f), Palette::Warning);
        g.Text(Loc::Get("hm_no_version"), { r.x, r.y + 68.f, r.w, 18.f }, kSub, Mono(11.f).Center());
        g.Text(Loc::Get("hm_no_version_hint"), { r.x, r.y + 88.f, r.w, 16.f }, kFaint, Mono(9.f).Center());
        return;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        StaggerItem in(ui, screenChangedAt + 0.05, static_cast<int>(i), 26.f * screenDirection, 0.f, 0.045, 0.38);
        const Rect slot = col.Next(60.f, 8.f);
        const Card c = DrawCard(ui, ui.MakeId("hand-version", static_cast<int64_t>(i)), slot, Palette::Teal);
        Badge(ui, c.r.x + 30.f, c.r.CY(), 16.f, std::wstring(1, rows[i].automatic ? Glyphs::Play : Glyphs::Folder), IconFont(12.f), Palette::Teal,
              c.hover);
        const float tx = c.r.x + 58.f + 3.f * c.hover;
        float right = c.r.R() - 30.f;
        if (rows[i].automatic) {
            right -= PillWidth(g, L"AUTO");
            Pill(g, right, c.r.y + 21.f, L"AUTO", Palette::Teal, true);
            right -= 8.f;
        }
        g.Text(rows[i].name, { tx, c.r.y + 12.f, right - tx, 18.f }, kText, Mono(12.f).SemiBold().Ellipsis());
        g.Text(rows[i].exe, { tx, c.r.y + 31.f, c.r.R() - 30.f - tx, 16.f }, kFaint, Mono(9.f).Ellipsis());
        Chevron(g, c.r, c.hover, Palette::Teal);
        if (c.it.clicked) SelectVersion(rows[i].exe);
    }
}

// ── Frame ───────────────────────────────────────────────────────────────────

void HandModsOverlay::Draw(Ui& ui, const Rect& screenRect) {
    auto& g = ui.g;
    std::wstring title = L"\x270B Hand Mods";
    if (screen == Screen::Versions) title = L"\x270B Hand Mods \x2014 Chapter " + std::to_wstring(chapter);
    else if (screen == Screen::Mods) title = L"\x270B Hand Mods \x2014 Select a Mod";
    else if (screen == Screen::Submit) title = Loc::Get("handmods_submit_header");
    else if (screen == Screen::Installing) title = L"\x270B Installing\x2026";

    {
        PanelStyle style;
        style.title = title;
        style.radius = 10.f;
        style.headerHeight = 52.f;
        style.footerHeight = 56.f;
        OverlayFrame frame(ui, ui.MakeId("handmods"), screenRect, 560.f, 540.f, style, popup.Opacity(), popup.Scale());

        // ← back (every screen but the hub and the install progress); the arrow nudges left on hover
        if (screen != Screen::Hub && screen != Screen::Installing) {
            const Rect row = frame.header.Inset(10.f, 0.f);
            const Rect b{ row.x, row.CY() - 17.f, 34.f, 34.f };
            const Interaction it = ui.Interact(ui.MakeId("hand-back"), b);
            const float hover = ui.Animate(ui.MakeId("hand-back-hover"), it.hovered ? 1.f : 0.f, 16.f);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            g.Border(b, 17.f, Lerp(Colors::Transparent, Palette::ButtonBg, hover), Lerp(Colors::Transparent, Palette::ControlBorder, hover), 1.f);
            TextStyle arrow = Mono(18.f);
            arrow.font = Font::Consolas;
            arrow.align = HAlign::Center;
            g.Text(L"\x2190", b.Offset(-3.f * hover, -1.f), Palette::Teal, arrow);
            if (it.clicked) {
                GoTo(screen == Screen::Mods ? Screen::Versions : Screen::Hub, -1);
                infoOpen = false;
                discord.CancelAuth();
            }
        }

        // Screen change: the new screen slides in from the side it was reached from
        const float slide = Anim::Clamp01(static_cast<float>((Async::Now() - screenChangedAt) / kSlide));
        if (slide < 1.f) ui.KeepAnimating();
        const auto pushSlide = [&] {
            if (slide >= 1.f) return false;
            g.PushOpacity(Anim::EaseOutCubic(std::min(1.f, slide * 1.6f)));
            ui.PushTranslate((1.f - Anim::EaseOutCubic(slide)) * 34.f * static_cast<float>(screenDirection), 0.f);
            return true;
        };
        const auto popSlide = [&](bool pushed) {
            if (!pushed) return;
            ui.PopTransform();
            g.PopLayer();
        };

        Rect body = frame.body;
        if (screen != Screen::Submit) {
            DrawSteps(ui, body.Top(kStepsH));
            body = { body.x, body.y + kStepsH, body.w, body.h - kStepsH };
        }
        if (screen == Screen::Installing) {
            const bool pushed = pushSlide();
            DrawInstalling(ui, body);
            popSlide(pushed);
        } else {
            const bool wide = screen == Screen::Hub || screen == Screen::Submit;
            ScrollArea scroll(ui, ui.MakeId("hand-scroll", static_cast<int>(screen)), body, false, 48.f, ScrollArea::Bar::Thin);
            Column col(body.x + (wide ? 22.f : 16.f), body.y + 14.f, scroll.ContentWidth() - (wide ? 44.f : 32.f));
            const bool pushed = pushSlide();
            switch (screen) {
                case Screen::Hub: DrawHub(ui, col); break;
                case Screen::Versions: DrawVersions(ui, col); break;
                case Screen::Mods: DrawMods(ui, col); break;
                case Screen::Submit: DrawSubmit(ui, col); break;
                default: break;
            }
            popSlide(pushed);
            scroll.SetContentSize(col.Height() + 28.f);
        }

        // Footer: a hint on the left, Close on the right
        const Rect row = frame.footer.Inset(16.f, 0.f);
        const std::wstring hint = screen == Screen::Hub         ? Loc::Get("hm_hint_hub")
                                  : screen == Screen::Versions  ? Loc::Get("hm_hint_versions")
                                  : screen == Screen::Submit    ? Loc::Get("hm_hint_submit")
                                                                : std::wstring();
        if (screen == Screen::Mods) {
            // Legend with the real icons
            float x = row.x;
            const struct { wchar_t glyph; std::wstring label; } legend[] = { { 0xE946, Loc::Get("hm_legend_info") }, { 0xEB9F, Loc::Get("hm_legend_preview") }, { 0xE8B7, Loc::Get("hm_legend_single") } };
            for (const auto& item : legend) {
                g.Text(std::wstring(1, item.glyph), { x, row.y, 14.f, row.h }, kFaint, IconFont(10.f));
                x += 18.f;
                const float w = g.Measure(item.label, Mono(9.f)).w + 1.f;
                g.Text(item.label, { x, row.y, w, row.h }, kFaint, Mono(9.f));
                x += w + 14.f;
            }
        } else if (!hint.empty()) {
            g.Text(hint, { row.x, row.y, row.w - 140.f, row.h }, kFaint, Mono(9.f).Ellipsis());
        }
        const TextStyle text = Mono(11.f, true).Center();
        const float w = ButtonWidth(ui, Loc::Get("close"), text, Styles::Footer());
        if (TextButton(ui, ui.MakeId("hand-close"), { row.R() - w, row.CY() - 18.f, w, 36.f }, Loc::Get("close"), text, Palette::Dim, Styles::Footer())) {
            discord.CancelAuth();
            infoOpen = false;
            viewer = {};
            extrasPanel.open = false;   // pending installs still finish (FinishExtra ignores a closed panel)
            if (conflict.open) {        // an open conflict question counts as Cancel
                auto done = std::move(conflict.done);
                conflict = {};
                if (done) done(false);
            }
            popup.Close();
        }
    }
    if (extrasPanel.open && popup.IsOpen()) DrawExtras(ui, screenRect);
    if (viewer.open && popup.IsOpen()) DrawPreview(ui, screenRect);
    if (conflict.open && popup.IsOpen()) DrawConflict(ui, screenRect);   // on top of everything
    if (infoOpen) DrawDiscordInfoPopup(ui, screenRect, "handmods_submit_discord_info_why_text", infoOpen);
}

} // namespace SpeedrunLauncher::Main
