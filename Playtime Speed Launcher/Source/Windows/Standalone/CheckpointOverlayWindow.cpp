#include "Windows/Standalone/CheckpointOverlayWindow.h"
#include <algorithm>
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace Saves = Services::SaveGames;

namespace {

// First chapter whose Steam exe is running (by process name), 1-based; 0 = none.
int DetectRunningChapter() {
    const auto& chapters = App::L().chapters;
    for (size_t i = 0; i < chapters.size(); ++i) {
        const auto& exe = chapters[i].gameExePath;
        if (!exe || exe->empty()) continue;
        if (!Process::FindByName(Files::FileNameWithoutExtension(*exe)).empty()) return static_cast<int>(i) + 1;
    }
    return 0;
}

std::wstring ChapterTitle(int n) { return Loc::Get("ch" + std::to_string(n) + "_title"); }

} // namespace

bool CheckpointOverlayWindow::Open() {
    detected = DetectRunningChapter();
    if (detected >= 4) {
        armed = true;   // no list: LOAD copies the chapter's start saves
    } else {
        for (int n = 1; n <= 3; ++n) {
            if (detected && n != detected) continue;
            Section s{ n, Saves::Checkpoints(n) };
            if (!s.rows.empty()) sections.push_back(std::move(s));
        }
    }

    // WindowState=Maximized over the primary monitor, transparent + topmost
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Checkpoints";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    Show(SW_SHOW);
    Activate();
    return true;
}

void CheckpointOverlayWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    if (ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }
    // Backdrop (click outside the panel closes)
    g.FillRect(screen, Argb(0x88000000));
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);

    const float width = detected ? 480.f : 520.f;
    const float rowH = 7.f + std::max(g.Measure(L"\xE74E", IconFont(11.f)).h, g.Measure(L"Ag", Mono(10.f, true)).h) + 7.f;
    const float labelH = g.Measure(L"Ag", Mono(9.f, true)).h;
    float listH = 0.f;
    for (size_t s = 0; s < sections.size(); ++s) {
        if (s > 0 && !detected) listH += 10.f + 1.f + 10.f;
        if (!detected) listH += labelH + 6.f;
        listH += sections[s].rows.size() * rowH + (sections[s].rows.size() - 1) * 3.f;
    }
    const float viewH = std::min(listH, 480.f);
    const float panelH = 48.f + 14.f + viewH + 12.f + 32.f + 4.f + 14.f;
    const Rect panel = screen.Center(width, panelH);
    ui.Block(panel);
    g.Border(panel, 6.f, Argb(0xEF09141E), Hex(0x153048), 1.f);

    // Header: title + hint
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 47.f };
    g.FillRound(header, 5.f, 5.f, 0.f, 0.f, Palette::HeaderBg);
    g.FillRect({ header.x, header.B(), header.w, 1.f }, Palette::Divider);
    g.Text(detected ? L"[ " + ChapterTitle(detected) + L" ]" : Loc::Get("hotkey_header"), header, Palette::Teal, Mono(13.f, true).Center());
    g.Text(Loc::Get("hotkey_hint"), header.Inset(14.f, 0.f), Palette::Faint, Mono(10.f).Right());

    // Rows
    const Rect viewport{ panel.x + 16.f, header.B() + 1.f + 14.f, panel.w - 32.f, viewH };
    {
        ScrollArea scroll(ui, ui.MakeId("checkpoint-list"), viewport, false, 48.f, ScrollArea::Bar::Classic);
        Column col(viewport.x, viewport.y, scroll.ContentWidth());
        for (size_t s = 0; s < sections.size(); ++s) {
            if (s > 0 && !detected) {
                col.Gap(10.f);
                g.FillRect({ col.x, col.y, col.w, 1.f }, Rgba(13, 32, 48, 60));
                col.Gap(1.f + 10.f);
            }
            if (!detected) g.Text(ChapterTitle(sections[s].chapter), col.Next(labelH, 6.f), Palette::Teal, Mono(9.f, true));
            for (size_t i = 0; i < sections[s].rows.size(); ++i) {
                if (i) col.Gap(3.f);
                const Rect r = col.Next(rowH);
                const Interaction it = ui.Interact(ui.MakeId("row", static_cast<int64_t>(s * 1000 + i)), r);
                const bool selected = selectedSection == static_cast<int>(s) && selectedRow == static_cast<int>(i);
                g.FillRound(r, 3.f, selected ? Rgba(0, 204, 170, 50) : it.hovered ? Rgba(255, 255, 255, 28) : Rgba(255, 255, 255, 12));
                g.Text(L"\xE74E", { r.x + 10.f, r.y, 12.f, r.h }, Rgba(0, 204, 170, 200), IconFont(11.f));
                g.Text(sections[s].rows[i].name, { r.x + 10.f + 11.f + 8.f, r.y, r.w - 39.f, r.h }, Rgba(200, 210, 220), Mono(10.f, true).Ellipsis());
                if (it.held && ui.In().pressed[0]) {
                    selectedSection = static_cast<int>(s);
                    selectedRow = static_cast<int>(i);
                }
            }
        }
        scroll.SetContentSize(listH);
    }

    // Buttons: LOAD (+ DELETE for Chapters 4-5) on the left, BACK on the right
    const Rect bar{ panel.x + 16.f, viewport.B() + 12.f, panel.w - 32.f, 32.f };
    const bool canLoad = armed || selectedRow >= 0;
    const std::wstring load = Loc::Get(detected >= 4 ? "hotkey_load_btn" : "hotkey_load_all_btn");
    const ButtonStyle loadStyle{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 3.f, { 14.f, 0.f }, 0.f, 32.f };
    const float loadW = ButtonWidthFor(13.f + 7.f + g.Measure(load, Mono(11.f, true)).w, loadStyle);
    if (GlyphTextButton(ui, ui.MakeId("load"), { bar.x, bar.y, loadW, 32.f }, Glyphs::Save, 13.f, Palette::Teal, load, Mono(11.f, true), Palette::Teal,
                        loadStyle, 7.f, canLoad)) {
        if (armed) Saves::LoadStartSaves(detected);
        else Saves::LoadCheckpoint(sections[static_cast<size_t>(selectedSection)].rows[static_cast<size_t>(selectedRow)]);
        Close();
        return;
    }
    if (detected >= 4) {
        const std::wstring del = Loc::Get("hotkey_delete_btn");
        const ButtonStyle delStyle{ Palette::DangerBg, Palette::DangerBorder, 1.f, 3.f, { 14.f, 0.f }, 0.f, 32.f };
        const float delW = ButtonWidthFor(13.f + 7.f + g.Measure(del, Mono(11.f, true)).w, delStyle);
        if (GlyphTextButton(ui, ui.MakeId("delete"), { bar.x + loadW + 8.f, bar.y, delW, 32.f }, Glyphs::Delete, 13.f, Palette::DangerText, del,
                            Mono(11.f, true), Palette::DangerText, delStyle, 7.f)) {
            Saves::DeleteSaves(detected);
            Close();
            return;
        }
    }
    const std::wstring back = Loc::Get("back");
    const ButtonStyle backStyle{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 3.f, { 14.f, 0.f }, 0.f, 32.f };
    const float backW = ButtonWidth(ui, back, Mono(11.f, true), backStyle);
    if (TextButton(ui, ui.MakeId("back"), { bar.R() - backW, bar.y, backW, 32.f }, back, Mono(11.f, true).Center(), Palette::Dim, backStyle)) {
        Close();
        return;
    }
    if (backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) Close();
}

} // namespace SpeedrunLauncher::Windows
