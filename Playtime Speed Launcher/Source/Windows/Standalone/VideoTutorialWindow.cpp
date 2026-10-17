#include "Windows/Standalone/VideoTutorialWindow.h"
#include <cmath>
#include "App/Launcher.h"
#include "Core/Text.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Standalone/ReportSkipWindow.h"
#include "UI/Widgets/Panel.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace VT = Services::VideoTutorials;

VideoTutorialWindow::VideoTutorialWindow() = default;
VideoTutorialWindow::~VideoTutorialWindow() { player.Destroy(); }

bool VideoTutorialWindow::Open() {
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Skips and Glitches";
    o.style = WS_POPUP | WS_CLIPCHILDREN;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    // Seasonal particles: idle frames at 30 fps (input / tweens still at full rate)
    if (Seasonal::IsSeasonal()) {
        SetContinuous(true, true);
        SetIdleFps(30.0);
    }
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    player.Create(hwnd, { true, L"Select a video", Rgba(26, 58, 85) });
    player.onOpened = [this] { loadingId.clear(); };
    player.onFailed = [this] {
        loadingId.clear();
        current = nullptr;
    };
    Show(SW_SHOW);
    Activate();
    return true;
}

void VideoTutorialWindow::Select(const Video& video) {
    selectedId = video.id;
    loadingId = video.id;
    current = nullptr;
    const auto id = VT::ExtractVideoId(video.url);
    if (!id) {
        player.ShowError(L"Could not read this video's URL.");
        return;
    }
    current = &video;
    player.Load(*id);
    if (auto* discord = App::L().discord.get()) discord->SetWatchingTutorial(video.title, video.chapter);
}

float VideoTutorialWindow::DrawInfo(const Rect& area) {
    g.FillRound(area, 0.f, 0.f, 8.f, 0.f, Hex(0x03070D));
    const Video* v = nullptr;
    for (const auto& video : VT::All())
        if (video.id == selectedId) v = &video;
    std::wstring title = L"No video selected", category, description;
    if (v) {
        title = v->title;
        if (!v->runCategories.empty()) {
            const std::wstring ver = v->version.empty() ? L"" : L"  \x00B7  " + v->version;
            const std::wstring rest = v->restrictions.empty() ? L"" : L"  \x00B7  " + v->restrictions;
            category = Text::Upper(v->chapter + L"  \x00B7  " + Text::Join(v->runCategories, L", ") + ver + rest);
            description = v->author.empty() ? v->description
                          : v->description.empty() ? L"Video by " + v->author
                                                   : L"Video by " + v->author + L"  \x00B7  " + v->description;
        } else {
            category = Text::Upper(v->chapter + L"  \x00B7  " + v->category);
            description = v->description;
        }
    }
    // Empty TextBlocks still take one line in WPF, so the category / description rows are always there
    float y = area.y + 10.f;
    const float x = area.x + 16.f, w = area.w - 32.f;
    y += Paragraph(ui, x, y, w, title, Mono(13.f, true), Hex(0xC8D8E4));
    y += 4.f;
    const float categoryH = g.Measure(L"Ag", Mono(9.f)).h;
    if (!category.empty()) g.Text(category, { x, y, w, categoryH }, Palette::Teal, Mono(9.f).Ellipsis());
    y += categoryH + 5.f;
    y += description.empty() ? g.Measure(L"Ag", Mono(10.f)).h : Paragraph(ui, x, y, w, description, Mono(10.f), Palette::Dim);
    return y + 16.f - area.y;   // Padding 16,10,16,16
}

void VideoTutorialWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    if (ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }
    if (current && ui.Focus() == 0 && ui.KeyPressed(VK_SPACE)) player.TogglePlay();

    g.FillRect(screen, Palette::Backdrop);
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);
    const Rect panel = screen.Center(1200.f, 740.f);
    if (openedAt < 0.0) openedAt = ui.Time();
    const float openT = static_cast<float>(ui.Time() - openedAt);
    PanelBackdropGlow(ui, panel, Palette::Teal, openT);
    PanelRise rise(ui, openT);
    PanelGlow(ui, panel, 8.f, Palette::Teal, openT);
    ui.Block(panel);
    g.Border(panel, 8.f, Argb(0xEF07101A), Hex(0x153048), 1.f);

    // Header: • SKIPS AND GLITCHES ............ [+] [✕]
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 49.f };
    g.FillRound(header, 7.f, 7.f, 0.f, 0.f, Hex(0x05090F));
    g.FillRect({ header.x, header.B(), header.w, 1.f }, Palette::Divider);
    const Rect row = header.Inset(16.f, 0.f);
    g.FillRound({ row.x, row.CY() - 4.f, 8.f, 8.f }, 4.f, Palette::Teal);
    g.Text(L"SKIPS AND GLITCHES", row.CutLeft(18.f), Palette::Teal, Mono(13.f, true));
    const ButtonStyle smallButton{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 4.f, { 0.f }, 0.f, 28.f };
    if (TextButton(ui, ui.MakeId("close"), { row.R() - 28.f, row.CY() - 14.f, 28.f, 28.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, smallButton)) {
        Close();
        return;
    }
    const bool reportClicked = TextButton(ui, ui.MakeId("report"), { row.R() - 28.f - 8.f - 28.f, row.CY() - 14.f, 28.f, 28.f }, L"+",
                                          Mono(14.f, true).WithFont(Font::Ui).Center(), Palette::Teal, smallButton);
    ui.Tooltip(ui.MakeId("report"), L"Report a skip that's missing from this list");
    if (reportClicked) {
        report = std::make_unique<ReportSkipWindow>();
        auto* raw = report.get();
        report->onClosed = [this, raw] {
            if (report.get() == raw) report.reset();
        };
        if (!report->Open(hwnd)) report.reset();
    }

    const Rect body{ panel.x + 1.f, header.B() + 1.f, panel.w - 2.f, panel.B() - 1.f - header.B() - 1.f };
    DrawSidebar({ body.x, body.y, 300.f, body.h });

    // Player area: video / seek / controls / info
    const Rect right{ body.x + 300.f, body.y, body.w - 300.f, body.h };
    // Info row is Auto-sized: use the height measured last frame
    const Rect playerArea{ right.x, right.y, right.w, right.h - infoHeight };
    player.Draw(ui, playerArea, DpiScale(), false, false, true);
    const float measured = DrawInfo({ right.x, playerArea.B(), right.w, infoHeight });
    if (std::abs(measured - infoHeight) > 0.5f) {
        infoHeight = measured;
        ui.KeepAnimating();   // lay out again with the new height
    }
    if (!loadingId.empty()) ui.KeepAnimating();
    // Seasonal theme: particles over the menu (the video itself is a child window on top), decorations on the top edge
    Seasonal::CardArt::DrawPanelParticles(g, panel, 8.f, ui.Time(), 0.6f);
    PanelDecor(ui, panel, 8.f, Palette::Teal, openT, header.B());
    Seasonal::CardArt::DrawPanelEdge(g, panel, ui.Time());
    if (PanelEntranceRunning(openT)) ui.KeepAnimating();
    if (backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) Close();
}

} // namespace SpeedrunLauncher::Windows
