#include "Windows/Standalone/BeginnerTutorialWindow.h"
#include <algorithm>
#include <cmath>
#include "App/Config.h"
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalTheme.h"
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
};
constexpr Card kCards[] = {
    { L"mNz257eeAec", L"ADD A INSTALLED VERSION" },
    { L"FAOt2QUJIv0", L"INSTALL A DEPOT AUTOMATICALLY" },
    { L"Nc4OVsv1cs0", L"INSTALL A DEPOT MANUAL" },
    { L"JwE_bhk91Rc", L"OPEN THE CHECKPOINT LOADER" },
};

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
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    player.Create(hwnd, { false, L"LOADING, PLEASE WAIT", Rgba(200, 80, 60, 200) });
    openedAt = Async::Now();
    cardsShownAt = openedAt + 0.1;   // the cards follow the panel in
    Show(SW_SHOW);
    Activate();
    return true;
}

void BeginnerTutorialWindow::ShowCards() {
    player.Stop();
    current = -1;
    cardsShownAt = ui.Time();
}

void BeginnerTutorialWindow::BeginClose() {
    if (closingAt < 0.0) closingAt = ui.Time();
}

void BeginnerTutorialWindow::DrawCards(const Rect& area) {
    // 2×2 grid of numbered cards
    const Rect grid = area.Inset(20.f, 18.f);
    const float gap = 16.f;
    const float cellW = (grid.w - gap) / 2.f, cellH = (grid.h - gap) / 2.f;
    bool anyLoading = false;
    for (int i = 0; i < 4; ++i) {
        const Rect cell{ grid.x + (i % 2) * (cellW + gap), grid.y + (i / 2) * (cellH + gap), cellW, cellH };
        // Left column slides in from the left, right column from the right, one card after another
        StaggerItem in(ui, cardsShownAt, i, i % 2 == 0 ? -44.f : 44.f, 0.f, 0.07, 0.42);

        const Interaction it = ui.Interact(ui.MakeId("card", i), cell);
        const float hover = ui.Animate(ui.MakeId("card-hover", i), it.hovered ? 1.f : 0.f, 14.f);
        const float press = ui.Animate(ui.MakeId("card-press", i), it.held ? 1.f : 0.f, 30.f);
        // Hover lifts the card; pressing pushes it back down a little
        const Rect card = cell.Offset(0.f, -5.f * hover + 2.f * press);
        if (hover > 0.01f) g.GlowRound(card, 10.f, Palette::Teal, 26.f, 0.45f * hover);
        g.Border(card, 10.f, Lerp(Rgba(10, 24, 36), Rgba(14, 32, 48), hover), Lerp(Palette::ButtonBorder, Palette::Teal, hover * 0.8f), 1.f);

        // Caption under a UniformToFill thumbnail
        const TextStyle caption = Mono(11.f, true).Wrap().Top();
        const float captionH = g.Measure(kCards[i].title, caption, card.w - 70.f).h + 34.f;
        const Rect image{ card.x + 1.f, card.y + 1.f, card.w - 2.f, card.h - 2.f - captionH };
        g.PushRoundClip(card.Inset(1.f), 9.f);
        if (auto thumb = RemoteImages::Get(std::wstring(Config::Links::YouTubeThumbnails) + kCards[i].id + L"/maxresdefault.jpg")) {
            // Slow zoom towards the viewer while hovered
            const float zoom = 1.f + 0.06f * hover;
            ui.PushScale(zoom, image.CX(), image.CY());
            g.DrawImage(*thumb, image, 1.f, Stretch::UniformToFill);
            ui.PopTransform();
        } else {
            // Loading shimmer: a soft highlight sweeping across the placeholder
            anyLoading = true;
            g.FillRect(image, Rgba(12, 28, 42));
            const float sweep = static_cast<float>(std::fmod(ui.Time() * 0.8 + i * 0.2, 1.4)) - 0.2f;
            g.FillLinear({ image.x + image.w * sweep - image.w * 0.3f, image.y, image.w * 0.6f, image.h }, 0.f,
                         { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 14) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
        }
        // Darken the bottom of the thumbnail into the caption
        g.FillLinear({ image.x, image.B() - 60.f, image.w, 60.f }, 0.f, { { 0.f, Rgba(5, 16, 26, 0) }, { 1.f, Rgba(5, 16, 26, 230) } }, { 0.f, 0.f },
                     { 0.f, 1.f });
        g.FillRect({ card.x + 1.f, image.B(), card.w - 2.f, captionH }, Rgba(5, 16, 26, 235));
        g.PopLayer();

        // Play button fading / growing in the middle of the thumbnail on hover
        if (hover > 0.01f) {
            const float r = 24.f + 4.f * hover;
            g.FillEllipse(image.CX(), image.CY(), r, r, Rgba(0, 0, 0, static_cast<int>(150 * hover)));
            g.StrokeEllipse(image.CX(), image.CY(), r, r, Palette::Teal.WithAlpha(hover), 2.f);
            Glyph(ui, Glyphs::Play, { image.CX() - r + 3.f, image.CY() - r, r * 2.f, r * 2.f }, 18.f, Colors::White.WithAlpha(hover));
        }

        // Step number badge on the thumbnail
        const Rect badge{ card.x + 14.f, card.y + 14.f, 30.f, 30.f };
        g.Border(badge, 15.f, Rgba(5, 16, 26, 220), Lerp(Rgba(58, 106, 138), Palette::Teal, hover), 1.5f);
        g.Text(std::to_wstring(i + 1), badge, Lerp(Rgba(200, 220, 235), Palette::Teal, hover), Mono(13.f, true).Center());

        // Title + "watch" hint
        const Rect text{ card.x + 16.f, image.B() + 12.f, card.w - 70.f, captionH - 24.f };
        g.Text(kCards[i].title, text, Rgba(214, 228, 238), caption);
        g.Text(L"WATCH \x2192", { card.R() - 86.f, image.B() + 12.f, 70.f, 16.f }, Palette::Teal.WithAlpha(0.45f + 0.55f * hover),
               Mono(9.f, true).Right());

        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) {
            current = i;
            player.Load(kCards[i].id);
        }
    }
    // Thumbnails arrive asynchronously: keep checking (at ~60 fps) only while one is missing,
    // and not forever if the download failed
    if (anyLoading && ui.Time() - openedAt < 20.0) ui.KeepAnimatingThrottled();
}

void BeginnerTutorialWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    const double now = ui.Time();

    // Closing: fade the whole panel out, then really close
    float closeT = 0.f;
    if (closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - closingAt) / 0.18));
        if (closeT >= 1.f) {
            Close();
            return;
        }
        ui.KeepAnimating();
    }

    if (closingAt < 0.0 && ui.KeyPressed(VK_ESCAPE)) {
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
    const Rect panel = screen.Center(1000.f, 680.f);
    const float sinceOpen = static_cast<float>(now - openedAt);
    PanelBackdropGlow(ui, panel, Palette::Teal, sinceOpen);
    const bool scaled = scale != 1.f;
    if (scaled) ui.PushScale(scale, panel.CX(), panel.CY());
    ui.Block(panel);
    PanelGlow(ui, panel, 10.f, Palette::Teal, sinceOpen);
    g.Border(panel, 10.f, Argb(0xF207101A), Hex(0x0D3020), 1.f);

    // Header: [←] • TITLE / subtitle ............ [✕]
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 58.f };
    g.FillRound(header, 9.f, 9.f, 0.f, 0.f, Hex(0x05090F));
    const Rect row = header.Inset(14.f, 0.f);
    const ButtonStyle smallButton{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f };
    float titleX = row.x;
    if (current >= 0) {
        if (TextButton(ui, ui.MakeId("back"), { row.x, row.CY() - 15.f, 30.f, 30.f }, std::wstring(1, Glyphs::Back), IconFont(11.f), Palette::Dim,
                       smallButton))
            ShowCards();
        titleX += 44.f;
    }
    // Pulsing dot before the title
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 3.0));
    g.FillEllipse(titleX + 4.f, row.CY() - 7.f, 7.f + 2.f * pulse, 7.f + 2.f * pulse, Palette::Teal.WithAlpha(0.15f + 0.15f * pulse));
    g.FillEllipse(titleX + 4.f, row.CY() - 7.f, 4.f, 4.f, Palette::Teal);
    const Rect titleArea{ titleX + 18.f, row.y + 9.f, row.w - (titleX - row.x) - 60.f, 22.f };
    g.Text(current >= 0 ? kCards[current].title : L"BEGINNER TUTORIALS", titleArea, Palette::Teal, Mono(14.f, true));
    g.Text(current >= 0 ? std::wstring(L"Video ") + std::to_wstring(current + 1) + L" of 4  \x00B7  Esc to go back"
                        : std::wstring(L"4 videos  \x00B7  start here if you're new"),
           { titleArea.x, titleArea.B(), titleArea.w, 18.f }, Rgba(90, 130, 160), Mono(10.f));
    if (TextButton(ui, ui.MakeId("close"), { row.R() - 30.f, row.CY() - 15.f, 30.f, 30.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, smallButton))
        BeginClose();

    const Rect body{ panel.x + 1.f, header.B() + 1.f, panel.w - 2.f, panel.B() - 1.f - header.B() - 1.f };
    if (current < 0) DrawCards(body);
    else player.Draw(ui, body, DpiScale(), true, true, false);
    if (current < 0 && player.HasVideo()) player.Stop();
    PanelDecor(ui, panel, 10.f, Palette::Teal, sinceOpen, header.B() - 1.f);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();

    if (scaled) ui.PopTransform();
    if (layered) g.PopLayer();
    if (closingAt < 0.0 && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) BeginClose();
}

} // namespace SpeedrunLauncher::Windows
