#include "Windows/Standalone/BeginnerTutorialWindow.h"
#include <algorithm>
#include <cmath>
#include "App/Config.h"
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Settings/ConfigFile.h"
#include "Windows/Standalone/Common/RemoteImages.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/Stagger.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;

namespace {

struct Card {
    const wchar_t* id;
    const wchar_t* title;
    const wchar_t* description;
};
constexpr Card kCards[] = {
    { L"mNz257eeAec", L"ADD A INSTALLED VERSION", L"Register a game version you already have on your PC so the launcher can play it." },
    { L"FAOt2QUJIv0", L"INSTALL A DEPOT AUTOMATICALLY", L"Download an old version with SteamCMD: the launcher does every step for you." },
    { L"Nc4OVsv1cs0", L"INSTALL A DEPOT MANUAL", L"Download an old version yourself from the Steam console, then add it here." },
    { L"JwE_bhk91Rc", L"OPEN THE CHECKPOINT LOADER", L"Load any checkpoint in-game with a hotkey to practise a section again and again." },
};
constexpr int kCount = 4;
constexpr const wchar_t* kWatchedFile = L"tutorials_watched.cfg";

constexpr float kRadius = 12.f, kHeaderH = 66.f;
constexpr float kCardH = 150.f, kCardGap = 16.f;
constexpr float kPathH = 40.f, kPad = 26.f;
constexpr float kPanelW = 1060.f, kPlaylistW = 300.f;
constexpr wchar_t kGraduationCap = 0xE7BE;   // Segoe MDL2 "Education"
constexpr Color kPanelBg = Argb(0xF207101A);
constexpr Color kPanelBorder = Hex(0x0D3020);
constexpr Color kText = Hex(0xD2DCE6);
constexpr Color kSub = Hex(0x5A8AA0);
constexpr Color kFaint = Hex(0x2A5A7A);
constexpr Color kGreen = Hex(0x00CC88);
constexpr Color kThumbBg = Hex(0x0C1C2A);

std::wstring Thumbnail(int i) { return std::wstring(Config::Links::YouTubeThumbnails) + kCards[i].id + L"/maxresdefault.jpg"; }

// Loading placeholder: a highlight sweeping across
void Shimmer(Graphics& g, const Rect& r, double time, int i) {
    g.FillRect(r, kThumbBg);
    const float sweep = static_cast<float>(std::fmod(time * 0.8 + i * 0.2, 1.4)) - 0.2f;
    g.FillLinear({ r.x + r.w * sweep - r.w * 0.3f, r.y, r.w * 0.6f, r.h }, 0.f,
                 { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 14) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
}

} // namespace

bool BeginnerTutorialWindow::Open() {
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Beginner Tutorials";
    o.style = WS_POPUP | WS_CLIPCHILDREN;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    if (Seasonal::IsSeasonal()) {          // seasonal particles: idle frames at 30 fps
        SetContinuous(true, true);
        SetIdleFps(30.0);
    }
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    player.Create(hwnd, { false, L"LOADING, PLEASE WAIT", Rgba(200, 80, 60, 200) });
    if (const auto saved = Config::ReadText(kWatchedFile))
        for (int i = 0; i < kCount && i < static_cast<int>(saved->size()); ++i) watched[static_cast<size_t>(i)] = (*saved)[static_cast<size_t>(i)] == L'1';
    openedAt = Async::Now();
    cardsShownAt = openedAt + 0.1;   // the cards follow the panel in
    Show(SW_SHOW);
    Activate();
    return true;
}

// The hotkey again / Alt+F4 / the owner closing it: fade out first
void BeginnerTutorialWindow::OnClosing(bool& cancel) {
    if (finished) return;
    cancel = true;
    BeginClose();
    Invalidate();
}

void BeginnerTutorialWindow::SaveWatched() const {
    std::wstring text;
    for (bool w : watched) text += w ? L'1' : L'0';
    Config::WriteText(kWatchedFile, text);
}

int BeginnerTutorialWindow::WatchedCount() const { return static_cast<int>(std::count(watched.begin(), watched.end(), true)); }

void BeginnerTutorialWindow::Play(int index) {
    current = index;
    viewChangedAt = Async::Now();
    player.Load(kCards[index].id);
    if (!watched[static_cast<size_t>(index)]) {
        watched[static_cast<size_t>(index)] = true;
        SaveWatched();
    }
}

void BeginnerTutorialWindow::ShowCards() {
    player.Stop();
    current = -1;
    cardsShownAt = viewChangedAt = Async::Now();
}

void BeginnerTutorialWindow::BeginClose() {
    if (closingAt >= 0.0) return;
    closingAt = Async::Now();
    player.Stop();   // the player window disappears first, the panel fades after it
}

// ── Path: ① ── ② ── ③ ── ④ with the watched part of the line lit ────────────

void BeginnerTutorialWindow::DrawPath(const Rect& area) {
    const double now = Async::Now();
    const float x0 = area.x + area.w / (kCount * 2.f), x1 = area.R() - area.w / (kCount * 2.f), cy = area.CY();
    g.FillRound({ x0, cy - 1.5f, x1 - x0, 3.f }, 1.5f, Palette::ButtonBorder);
    // Lit up to the last watched step (fills in after opening)
    int last = -1;
    for (int i = 0; i < kCount; ++i)
        if (watched[static_cast<size_t>(i)]) last = i;
    const float target = last <= 0 ? 0.f : static_cast<float>(last) / (kCount - 1);
    const float fill = ui.Animate(ui.MakeId("path-fill"), target, 4.f, 0.f);
    if (fill > 0.001f) {
        g.FillLinear({ x0, cy - 1.5f, (x1 - x0) * fill, 3.f }, 1.5f, { { 0.f, Palette::ConfirmBorder }, { 1.f, kGreen } }, { 0.f, 0.f }, { 1.f, 0.f });
        g.GlowRound({ x0, cy - 1.5f, (x1 - x0) * fill, 3.f }, 1.5f, kGreen, 8.f, 0.4f);
    }
    // The next step to watch pulses for a few seconds
    int next = -1;
    for (int i = 0; i < kCount; ++i)
        if (!watched[static_cast<size_t>(i)]) {
            next = i;
            break;
        }
    const float calm = std::clamp(5.f - static_cast<float>(now - openedAt), 0.f, 1.f);
    if (calm > 0.f) ui.KeepAnimatingThrottled();
    for (int i = 0; i < kCount; ++i) {
        StaggerItem in(ui, openedAt + 0.1, i, 0.f, -10.f, 0.06, 0.36);
        const float x = x0 + (x1 - x0) * i / (kCount - 1);
        const bool done = watched[static_cast<size_t>(i)];
        const bool playing = i == current;
        if (i == next && calm > 0.f) {
            const float p = static_cast<float>(std::fmod(now, 1.6) / 1.6);
            g.StrokeEllipse(x, cy, 14.f + 10.f * p, 14.f + 10.f * p, Palette::Teal.WithAlpha(0.5f * (1.f - p) * calm), 1.5f);
        }
        const Color ring = done ? kGreen : playing || i == next ? Palette::Teal : Palette::ControlBorder;
        g.FillEllipse(x, cy, 14.f, 14.f, done ? kGreen.WithAlpha(0.2f) : Palette::ButtonBg);
        g.StrokeEllipse(x, cy, 14.f, 14.f, ring, playing ? 2.5f : 1.5f);
        if (done) g.Text(std::wstring(1, Glyphs::CheckMark), { x - 14.f, cy - 14.f, 28.f, 28.f }, kGreen, IconFont(10.f));
        else g.Text(std::to_wstring(i + 1), { x - 14.f, cy - 14.f, 28.f, 28.f }, ring, Mono(11.f, true).Center());
    }
}

// ── Cards: 2 × 2, thumbnail on the left, text and the action on the right ──

void BeginnerTutorialWindow::DrawCards(const Rect& area) {
    const double now = Async::Now();
    const float cardW = (area.w - kCardGap) / 2.f;
    int next = -1;   // first one not watched yet gets "UP NEXT"
    for (int i = 0; i < kCount; ++i)
        if (!watched[static_cast<size_t>(i)]) {
            next = i;
            break;
        }
    bool anyLoading = false;
    for (int i = 0; i < kCount; ++i) {
        const Rect cell{ area.x + (i % 2) * (cardW + kCardGap), area.y + (i / 2) * (kCardH + kCardGap), cardW, kCardH };
        // Left column slides in from the left, right column from the right, one after another
        StaggerItem in(ui, cardsShownAt, i, i % 2 == 0 ? -36.f : 36.f, 0.f, 0.07, 0.42);

        const Interaction it = ui.Interact(ui.MakeId("card", i), cell);
        const float hover = ui.Animate(ui.MakeId("card-hover", i), it.hovered ? 1.f : 0.f, 14.f);
        const float press = ui.Animate(ui.MakeId("card-press", i), it.held ? 1.f : 0.f, 30.f);
        const bool done = watched[static_cast<size_t>(i)];
        const bool upNext = i == next;
        const Color accent = done ? kGreen : Palette::Teal;
        // Hover lifts the card; pressing pushes it back down a little
        const Rect card = cell.Offset(0.f, -4.f * hover + 2.f * press);
        if (hover > 0.01f || upNext) g.GlowRound(cell, 12.f, accent, 20.f, std::max(0.4f * hover, upNext ? 0.18f : 0.f));
        const Color border = done ? kGreen.WithAlpha(0.35f) : upNext ? Palette::Teal.WithAlpha(0.55f) : Palette::ButtonBorder;
        g.Border(card, 12.f, Lerp(Rgba(10, 24, 36), Rgba(14, 32, 48), hover), Lerp(border, accent, hover * 0.85f), 1.f);

        // Thumbnail (16:9) on the left, zooming in slowly while hovered
        const float thumbH = card.h - 24.f;
        const Rect image{ card.x + 12.f, card.y + 12.f, thumbH * 16.f / 9.f, thumbH };
        g.PushRoundClip(image, 8.f);
        if (auto thumb = RemoteImages::Get(Thumbnail(i))) {
            ui.PushScale(1.f + 0.07f * hover, image.CX(), image.CY());
            g.DrawImage(*thumb, image, done ? 0.7f + 0.3f * hover : 1.f, Stretch::UniformToFill);
            ui.PopTransform();
        } else {
            anyLoading = true;
            Shimmer(g, image, now, i);
        }
        g.PopLayer();
        g.StrokeRound(image, 8.f, Rgba(255, 255, 255, 18), 1.f);
        // Play button: always there, grows and lights up on hover
        {
            const float r = 17.f + 4.f * hover;
            g.FillEllipse(image.CX(), image.CY(), r, r, Rgba(0, 0, 0, static_cast<int>(110 + 60 * hover)));
            g.StrokeEllipse(image.CX(), image.CY(), r, r, accent.WithAlpha(0.5f + 0.5f * hover), 1.5f + 0.5f * hover);
            Glyph(ui, Glyphs::Play, { image.CX() - r + 2.f, image.CY() - r, r * 2.f, r * 2.f }, 12.f + 3.f * hover, Colors::White.WithAlpha(0.8f + 0.2f * hover));
        }
        // Step number on the thumbnail
        const Rect badge{ image.x + 8.f, image.y + 8.f, 26.f, 26.f };
        g.Border(badge, 13.f, Rgba(5, 16, 26, 220), done ? kGreen : Lerp(Rgba(58, 106, 138), accent, hover), 1.5f);
        if (done) g.Text(std::wstring(1, Glyphs::CheckMark), badge, kGreen, IconFont(10.f));
        else g.Text(std::to_wstring(i + 1), badge, Lerp(Rgba(200, 220, 235), accent, hover), Mono(12.f, true).Center());

        // Text column: STEP n + state pill, title, what it teaches, action
        const float tx = image.R() + 16.f + 3.f * hover;
        const Rect text{ tx, card.y + 14.f, card.R() - 16.f - tx, card.h - 28.f };
        g.Text(L"STEP " + std::to_wstring(i + 1), { text.x, text.y, 80.f, 14.f }, done ? kGreen : kFaint, Mono(8.f, true));
        if (done || upNext) {
            const std::wstring label = done ? L"WATCHED" : L"UP NEXT";
            const TextStyle style = Mono(7.f, true).Center();
            const float pw = g.Measure(label, style).w + 14.f;
            const Rect pill{ text.R() - pw, text.y - 2.f, pw, 17.f };
            g.Border(pill, 8.5f, accent.WithAlpha(done ? 0.14f : 0.18f), accent.WithAlpha(0.6f), 1.f);
            g.Text(label, pill, accent, style);
        }
        TextStyle title = Mono(11.f, true).Wrap().Top();
        const float titleH = std::min(g.Measure(kCards[i].title, title, text.w).h, 32.f);
        g.Text(kCards[i].title, { text.x, text.y + 20.f, text.w, titleH }, Lerp(kText, Colors::White, hover), title);
        g.Text(kCards[i].description, { text.x, text.y + 24.f + titleH, text.w, text.B() - 22.f - (text.y + 24.f + titleH) }, kSub, Mono(9.f).Wrap().Top());
        g.Text(done ? L"WATCH AGAIN  →" : L"WATCH  →", { text.x + 4.f * hover, text.B() - 14.f, text.w, 14.f },
               accent.WithAlpha(0.55f + 0.45f * hover), Mono(9.f, true));

        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) Play(i);
    }
    // Thumbnails arrive asynchronously: keep checking (at ~60 fps) only while one is missing,
    // and not forever if the download failed
    if (anyLoading && now - openedAt < 20.0) ui.KeepAnimatingThrottled();
}

// ── Video view: the four steps beside the player, plus "next" ───────────────

void BeginnerTutorialWindow::DrawPlaylist(const Rect& area) {
    const double now = Async::Now();
    g.FillRect(area, Rgba(5, 10, 18, 120));
    g.FillRect({ area.x, area.y, 1.f, area.h }, Palette::Divider);
    g.Text(L"BEGINNER PATH", { area.x + 18.f, area.y + 16.f, area.w - 36.f, 14.f }, kSub, Mono(9.f, true));
    float y = area.y + 40.f;
    for (int i = 0; i < kCount; ++i) {
        StaggerItem in(ui, viewChangedAt, i, 18.f, 0.f, 0.05, 0.32);
        const Rect r{ area.x + 12.f, y, area.w - 24.f, 74.f };
        const Interaction it = ui.Interact(ui.MakeId("playlist", i), r);
        const bool playing = i == current;
        const float hover = ui.Animate(ui.MakeId("playlist-hover", i), it.hovered && !playing ? 1.f : 0.f, 16.f);
        const float sel = ui.Animate(ui.MakeId("playlist-sel", i), playing ? 1.f : 0.f, 12.f);
        g.Border(r, 10.f, Lerp(Lerp(Rgba(255, 255, 255, 6), Rgba(255, 255, 255, 16), hover), Rgba(0, 204, 170, 28), sel),
                 Lerp(Lerp(Palette::ButtonBorder, Rgba(40, 90, 120, 160), hover), Palette::Teal, sel), 1.f);
        const Rect thumb{ r.x + 8.f, r.y + 8.f, (r.h - 16.f) * 16.f / 9.f, r.h - 16.f };
        if (auto img = RemoteImages::Get(Thumbnail(i))) g.DrawImageRounded(*img, thumb, 6.f, playing ? 1.f : 0.75f + 0.25f * hover);
        else g.FillRound(thumb, 6.f, kThumbBg);
        if (playing) {
            // Equalizer bars on the playing one
            for (int b = 0; b < 3; ++b) {
                const float h = 6.f + 8.f * (0.5f + 0.5f * std::sin(static_cast<float>(now) * 6.f + b * 1.7f));
                g.FillRound({ thumb.x + 8.f + b * 6.f, thumb.B() - 6.f - h, 4.f, h }, 1.f, Palette::Teal);
            }
            ui.KeepAnimatingThrottled();
        }
        const float tx = thumb.R() + 12.f;
        g.Text(L"STEP " + std::to_wstring(i + 1) + (watched[static_cast<size_t>(i)] && !playing ? L"  \x2713" : L""), { tx, r.y + 12.f, r.R() - tx - 8.f, 14.f },
               playing ? Palette::Teal : watched[static_cast<size_t>(i)] ? kGreen : kFaint, Mono(8.f, true));
        g.Text(kCards[i].title, { tx, r.y + 28.f, r.R() - tx - 8.f, 36.f }, Lerp(kSub, kText, std::max(sel, hover)), Mono(9.f, true).Wrap().Top());
        if (it.hovered && !playing) ui.SetCursor(IDC_HAND);
        if (it.clicked && !playing) Play(i);
        y += 74.f + 8.f;
    }
    // Next step
    if (current >= 0 && current + 1 < kCount) {
        const Rect b{ area.x + 12.f, area.B() - 16.f - 40.f, area.w - 24.f, 40.f };
        const ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 8.f, { 0.f }, 0.f, 40.f };
        g.GlowRound(b, 8.f, Palette::Teal, 14.f, 0.22f);
        if (TextButton(ui, ui.MakeId("next"), b, L"NEXT: STEP " + std::to_wstring(current + 2) + L"  \x2192", Mono(10.f, true).Center(), Palette::Teal, s))
            Play(current + 1);
    }
}

// ── Frame ───────────────────────────────────────────────────────────────────

void BeginnerTutorialWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    const double now = Async::Now();

    // Closing: fade the whole panel out, then really close
    float closeT = 0.f;
    if (closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - closingAt) / 0.18));
        if (closeT >= 1.f) {
            finished = true;
            Close();
            return;
        }
        ui.KeepAnimating();
    }
    const bool interactive = closingAt < 0.0;
    if (interactive && ui.KeyPressed(VK_ESCAPE)) {
        if (current >= 0) ShowCards();
        else BeginClose();
    }
    if (current >= 0 && ui.KeyPressed(VK_SPACE)) player.TogglePlay();

    // Pop-in: fade + scale 0.92 → 1 with a slight overshoot; pop-out: fade + shrink
    const float openT = Anim::Clamp01(static_cast<float>((now - openedAt) / 0.34));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.4f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.92f, 1.f, Anim::EaseOutBack(openT, 0.7f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);

    g.FillRect(screen, Palette::Backdrop);
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);
    // Sized to what it shows: the cards, or a 16:9 video with its controls beside the step list (animated)
    const float panelW = std::min(kPanelW, screen.w - 40.f);
    const float cardsH = kHeaderH + 1.f + kPad + kPathH + 22.f + 2.f * kCardH + kCardGap + kPad;
    const float videoH = kHeaderH + 1.f + (panelW - 2.f - kPlaylistW) * 9.f / 16.f + 22.f + 52.f + 1.f;
    const float panelH = std::min(ui.Animate(ui.MakeId("panel-height"), current < 0 ? cardsH : videoH, 12.f, cardsH), screen.h - 40.f);
    const Rect panel = screen.Center(panelW, panelH);
    const float sinceOpen = static_cast<float>(now - openedAt);
    PanelBackdropGlow(ui, panel, Palette::Teal, sinceOpen);
    const bool scaled = scale != 1.f;
    if (scaled) ui.PushScale(scale, panel.CX(), panel.CY());
    ui.Block(panel);
    PanelGlow(ui, panel, kRadius, Palette::Teal, sinceOpen);
    g.Border(panel, kRadius, kPanelBg, kPanelBorder, 1.f);
    g.FillLinear(panel.Inset(1.f).Top(220.f), kRadius - 1.f, { { 0.f, Palette::Teal.WithAlpha(0.06f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } },
                 { 0.f, 0.f }, { 0.f, 1.f });

    // ── Header: [←] 🎓 TITLE / subtitle ..... [2/4 ▓▓░░] [✕] ─────────────────
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Hex(0x05090F));
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    const Rect row = header.Inset(16.f, 0.f);
    const ButtonStyle smallButton{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 16.f, { 0.f }, 0.f, 32.f };
    float titleX = row.x;
    if (current >= 0) {
        if (TextButton(ui, ui.MakeId("back"), { row.x, row.CY() - 16.f, 32.f, 32.f }, std::wstring(1, Glyphs::Back), IconFont(11.f), Palette::Dim,
                       smallButton))
            ShowCards();
        titleX += 44.f;
    }
    {
        StaggerItem in(ui, openedAt + 0.06, 0, -14.f, 0.f, 0.0, 0.36);
        const Rect badge{ titleX, row.CY() - 19.f, 38.f, 38.f };
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 2.4)) * std::clamp(5.f - sinceOpen, 0.f, 1.f);
        g.GlowRound(badge, 10.f, Palette::Teal, 14.f, 0.15f + 0.2f * pulse);
        g.Border(badge, 10.f, Rgba(0, 204, 170, 26), Rgba(0, 204, 170, 130), 1.f);
        g.Text(std::wstring(1, kGraduationCap), badge, Palette::Teal, IconFont(16.f));
        const float tx = badge.R() + 14.f;
        g.Text(current >= 0 ? kCards[current].title : L"BEGINNER TUTORIALS", { tx, header.y + 14.f, row.w - (tx - row.x) - 260.f, 20.f }, Palette::Teal,
               Mono(14.f, true).Ellipsis());
        g.Text(current >= 0 ? L"Step " + std::to_wstring(current + 1) + L" of " + std::to_wstring(kCount) + L"  \x00B7  Esc to go back"
                            : std::wstring(L"Start here if you're new  \x00B7  4 short videos"),
               { tx, header.y + 36.f, row.w - (tx - row.x) - 260.f, 16.f }, kSub, Mono(9.f, true));
    }
    if (TextButton(ui, ui.MakeId("close"), { row.R() - 32.f, row.CY() - 16.f, 32.f, 32.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, smallButton) &&
        interactive)
        BeginClose();
    {
        // Progress: "2 / 4 WATCHED" and a bar filling up
        const int count = WatchedCount();
        const Rect bar{ row.R() - 32.f - 20.f - 170.f, row.CY() + 6.f, 170.f, 5.f };
        const std::wstring label = count == kCount ? L"ALL WATCHED \x2713" : std::to_wstring(count) + L" / " + std::to_wstring(kCount) + L" WATCHED";
        g.Text(label, { bar.x, row.CY() - 14.f, bar.w, 14.f }, count == kCount ? kGreen : kSub, Mono(8.f, true).Right());
        const float shown = ui.Animate(ui.MakeId("progress"), static_cast<float>(count) / kCount, 6.f, 0.f);
        g.FillRound(bar, 2.5f, Palette::ButtonBorder);
        if (shown > 0.001f) g.FillLinear({ bar.x, bar.y, bar.w * shown, bar.h }, 2.5f, { { 0.f, Palette::ConfirmBorder }, { 1.f, kGreen } }, { 0.f, 0.f },
                                         { 1.f, 0.f });
    }

    // ── Body: cross-fades between the cards and the video ───────────────────
    const Rect body{ panel.x + 1.f, header.B() + 1.f, panel.w - 2.f, panel.B() - 1.f - header.B() - 1.f };
    const float change = Anim::Clamp01(static_cast<float>((now - viewChangedAt) / 0.3));
    if (change < 1.f) ui.KeepAnimating();
    if (current < 0) {
        const bool fading = change < 1.f;   // a layer only while it fades in
        if (fading) g.PushOpacity(Anim::EaseOutCubic(change));
        DrawPath({ body.x + kPad, body.y + kPad, body.w - 2.f * kPad, kPathH });
        DrawCards({ body.x + kPad, body.y + kPad + kPathH + 22.f, body.w - 2.f * kPad, 2.f * kCardH + kCardGap });
        if (fading) g.PopLayer();
    } else {
        const float listW = kPlaylistW;
        DrawPlaylist({ body.R() - listW, body.y, listW, body.h });
        player.Draw(ui, { body.x, body.y, body.w - listW, body.h }, DpiScale(), true, false, false);
    }
    if (current < 0 && player.HasVideo()) player.Stop();

    // Seasonal theme: particles over the content (the video itself is a child window on top), decorations on the edge
    Seasonal::CardArt::DrawPanelParticles(g, panel, kRadius, now, 0.5f);
    PanelDecor(ui, panel, kRadius, Palette::Teal, sinceOpen, header.B() - 1.f);
    Seasonal::CardArt::DrawPanelEdge(g, panel, now);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();

    if (scaled) ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) BeginClose();
}

} // namespace SpeedrunLauncher::Windows
