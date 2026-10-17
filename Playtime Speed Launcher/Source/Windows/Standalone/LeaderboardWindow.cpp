#include "Windows/Standalone/LeaderboardWindow.h"
#include <algorithm>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/Panel.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace SC = Services::SpeedrunCom;

namespace {

constexpr Color kDim = Rgba(58, 106, 138);

std::wstring RunnersKey(const SC::RunType& r) { return r.gameId + L"|" + r.categoryId + L"|" + SC::BuildVarQuery(r); }

float Status(Ui& ui, float x, float y, float width, const std::wstring& text, Color color, float size = 12.f) {
    const float h = ui.g.Measure(L"Ag", Mono(size)).h;
    ui.g.Text(text, { x + 4.f, y + 10.f, width - 4.f, h }, color, Mono(size));
    return 10.f + h;
}

} // namespace

bool LeaderboardWindow::Open() {
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Leaderboards";
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
    if (!App::L().chapters.empty()) SelectChapter(App::L().chapters.front().number);
    return true;
}

void LeaderboardWindow::SelectChapter(int number) {
    chapter = number;
    openSelector.clear();
    if (runTypes.count(number) || loadingChapters.count(number)) return;
    loadingChapters.insert(number);
    auto guard = alive;
    Async::Run([number] { return SC::GetRunTypes(number); }, [this, guard, number](std::vector<SC::RunType> list) {
        if (!*guard) return;
        loadingChapters.erase(number);
        runTypes[number] = std::move(list);
        Invalidate();
    });
}

void LeaderboardWindow::RequestRunners(const SC::RunType& runType) {
    const std::wstring key = RunnersKey(runType);
    if (runners.count(key) || loadingRunners.count(key)) return;
    loadingRunners.insert(key);
    auto guard = alive;
    Async::Run([runType] { return SC::GetTopRunners(runType); }, [this, guard, key](std::vector<SC::LeaderboardEntry> list) {
        if (!*guard) return;
        loadingRunners.erase(key);
        runners[key] = std::move(list);
        Invalidate();
    });
}

float LeaderboardWindow::DrawCard(SC::RunType& runType, size_t index, float x, float y, float width, float scrollOffset) {
    const float innerX = x + 15.f, innerW = width - 30.f;
    // Measure: title, selectors (wrapped), runners
    const float titleH = g.Measure(L"Ag", Mono(13.f, true)).h;
    std::vector<Rect> selectorRects;
    float cy = y + 11.f + titleH + 8.f;
    if (!runType.secondaryVariables.empty()) {
        float sx = innerX, rowY = cy;
        for (const auto& v : runType.secondaryVariables) {
            const std::wstring label = Text::Upper(v.name) + L":";
            std::wstring selected = v.values.empty() ? L"" : v.values.front().label;
            for (const auto& value : v.values)
                if (value.id == v.selectedValueId) selected = value.label;
            const float w = g.Measure(label, Mono(9.f, true)).w + 6.f + 8.f + g.Measure(selected, Mono(10.f).SemiBold()).w + 6.f + 10.f + 8.f + 2.f;
            if (sx > innerX && sx + w > innerX + innerW) {
                sx = innerX;
                rowY += 24.f + 6.f;
            }
            selectorRects.push_back({ sx, rowY, w, 24.f });
            sx += w + 10.f;
        }
        cy = rowY + 24.f + 6.f + 10.f;
    }
    const std::wstring key = RunnersKey(runType);
    const auto found = runners.find(key);
    const float rowH = 5.f + std::max(g.Measure(L"Ag", Mono(12.f)).h, 22.f) + 5.f + 1.f;
    float runnersH;
    if (found == runners.end() || found->second.empty()) runnersH = 10.f + g.Measure(L"Ag", Mono(11.f)).h;
    else runnersH = rowH * found->second.size();
    const Rect card{ x, y, width, 11.f + titleH + 8.f + (cy - (y + 11.f + titleH + 8.f)) + runnersH + 13.f };
    g.Border(card, 6.f, Rgba(9, 20, 30, 160), Rgba(17, 34, 51, 120), 1.f);
    g.Text(runType.label, { innerX, y + 11.f, innerW, titleH }, Palette::Teal, Mono(13.f, true));

    // Selectors (themed dropdowns)
    for (size_t i = 0; i < runType.secondaryVariables.size(); ++i) {
        auto& v = runType.secondaryVariables[i];
        const Rect r = selectorRects[i];
        const std::wstring label = Text::Upper(v.name) + L":";
        const float labelW = g.Measure(label, Mono(9.f, true)).w;
        g.Text(label, { r.x, r.y, labelW + 1.f, r.h }, kDim, Mono(9.f, true));
        std::wstring selected = v.values.empty() ? L"" : v.values.front().label;
        for (const auto& value : v.values)
            if (value.id == v.selectedValueId) selected = value.label;
        const Rect button{ r.x + labelW + 6.f, r.y, r.R() - (r.x + labelW + 6.f), 24.f };
        const std::wstring selectorId = std::to_wstring(index) + L"|" + v.id;
        {
            ButtonScope b(ui, ui.MakeId("selector", static_cast<int64_t>(index * 100 + i)), button,
                          { Rgba(10, 24, 37), Rgba(26, 58, 85), 1.f, 4.f, { 8.f, 0.f }, 0.f, 24.f });
            const float tw = g.Measure(selected, Mono(10.f).SemiBold()).w + 1.f;
            g.Text(selected, { b.content.x, r.y, tw, 24.f }, Palette::Label, Mono(10.f).SemiBold());
            g.Text(L"\xE70D", { b.content.x + tw + 6.f, r.y, 10.f, 24.f }, Rgba(42, 90, 122), IconFont(8.f));
            if (b.Clicked()) openSelector = openSelector == selectorId ? L"" : selectorId;
        }
        if (openSelector == selectorId) {
            SC::RunType* type = &runType;
            SC::Variable* variable = &v;
            // Deferred drawing runs outside the scroll transform: convert to window coordinates
            const Rect anchor{ button.x, button.y - scrollOffset, button.w, button.h };
            ui.Defer([this, anchor, type, variable] {
                const Rect button = anchor;
                auto& gr = ui.g;
                const float itemH = gr.Measure(L"Ag", Mono(11.f)).h + 12.f;
                float w = 120.f;
                for (const auto& value : variable->values) w = std::max(w, gr.Measure(value.label, Mono(11.f)).w + 30.f + 2.f);
                const Rect list{ button.x, button.B(), w, itemH * variable->values.size() + 2.f };
                ui.Block(list);
                gr.Border(list, 4.f, Rgba(9, 20, 30), Rgba(21, 48, 72), 1.f);
                float iy = list.y + 1.f;
                int n = 0;
                for (const auto& value : variable->values) {
                    const Rect item{ list.x + 1.f, iy, list.w - 2.f, itemH };
                    ButtonScope b(ui, ui.MakeId("selector-option", n++), item, Styles::Flat());
                    gr.Text(value.label, item.Inset(10.f, 0.f), Palette::Label, Mono(11.f));
                    if (b.Clicked()) {
                        variable->selectedValueId = value.id;
                        openSelector.clear();
                        RequestRunners(*type);
                    }
                    iy += itemH;
                }
                if (ui.In().pressed[0] && !ui.MouseOver(list) && !ui.MouseOver(button)) openSelector.clear();
            });
        }
    }

    // Runners
    RequestRunners(runType);
    float ry = cy;
    if (found == runners.end()) {
        Status(ui, innerX, ry, innerW, L"Cargando...", kDim, 11.f);
    } else if (found->second.empty()) {
        Status(ui, innerX, ry, innerW, L"Sin runs registradas todav\x00EDa.", kDim, 11.f);
    } else {
        int n = 0;
        for (const auto& entry : found->second) {
            const Rect row{ innerX, ry, innerW, rowH };
            g.FillRect({ row.x, row.B() - 1.f, row.w, 1.f }, Rgba(17, 34, 51, 60));
            const Rect content{ row.x, row.y + 5.f, row.w, row.h - 11.f };
            const Color rank = entry.place == 1 ? Rgba(255, 210, 77) : entry.place == 2 ? Rgba(200, 210, 220) : entry.place == 3 ? Rgba(205, 140, 95) : kDim;
            g.Text(L"#" + std::to_wstring(entry.place), { content.x, content.y, 32.f, content.h }, rank, Mono(11.f, true));
            const std::wstring time = SC::FormatTime(entry.timeSeconds);
            const float timeW = g.Measure(time, Mono(12.f).SemiBold()).w + 1.f;
            const Rect link{ content.R() - 22.f, content.CY() - 11.f, 22.f, 22.f };
            g.Text(time, { link.x - 10.f - timeW, content.y, timeW, content.h }, Palette::Teal, Mono(12.f).SemiBold());
            g.Text(entry.playerName, { content.x + 32.f + 6.f, content.y, link.x - 10.f - timeW - 6.f - (content.x + 38.f), content.h }, Rgba(220, 230, 240),
                   Mono(12.f).Ellipsis());
            const Id linkId = ui.MakeId("run-link", static_cast<int64_t>(index * 1000 + n++));
            if (TextButton(ui, linkId, link, L"\xE8A7", IconFont(12.f), kDim, Styles::Flat()) && !entry.runWeblink.empty())
                Process::OpenUrl(entry.runWeblink);
            ui.Tooltip(linkId, L"Ver run en speedrun.com");
            ry += rowH;
        }
    }
    return card.h;
}

void LeaderboardWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    if (ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }
    g.FillRect(screen, Palette::Backdrop);
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);
    const Rect panel = screen.Center(1150.f, 760.f);
    if (openedAt < 0.0) openedAt = ui.Time();
    const float openT = static_cast<float>(ui.Time() - openedAt);
    PanelBackdropGlow(ui, panel, Palette::Teal, openT);
    PanelRise rise(ui, openT);
    PanelGlow(ui, panel, 8.f, Palette::Teal, openT);
    ui.Block(panel);
    g.Border(panel, 8.f, Argb(0xEF07101A), Hex(0x0D3020), 1.f);

    // Header
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 49.f };
    g.FillRound(header, 7.f, 7.f, 0.f, 0.f, Hex(0x05090F));
    g.FillRect({ header.x, header.B(), header.w, 1.f }, Palette::Divider);
    g.FillRound({ header.x + 16.f, header.CY() - 4.f, 8.f, 8.f }, 4.f, Palette::Teal);
    g.Text(L"SPEEDRUN.COM LEADERBOARDS", header.CutLeft(16.f + 16.f), Palette::Teal, Mono(13.f, true));
    if (TextButton(ui, ui.MakeId("close"), { header.R() - 12.f - 28.f, header.CY() - 14.f, 28.f, 28.f }, std::wstring(1, Glyphs::Cancel), IconFont(12.f),
                   Palette::Dim, { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 4.f, { 0.f }, 0.f, 28.f })) {
        Close();
        return;
    }

    const Rect body{ panel.x + 16.f, header.B() + 1.f + 16.f, panel.w - 32.f, panel.B() - header.B() - 1.f - 32.f };
    // Chapters (210 px column)
    {
        const Rect column{ body.x, body.y, 210.f, body.h };
        ScrollArea scroll(ui, ui.MakeId("chapters"), column, false, 48.f, ScrollArea::Bar::Classic);
        Column col(column.x, column.y, scroll.ContentWidth());
        int chapterIndex = 0;
        for (const auto& ch : App::L().chapters) {
            StaggerItem in(ui, openedAt + 0.12, chapterIndex++, -26.f, 0.f, 0.05);
            const bool selected = ch.number == chapter;
            const std::wstring subtitle = ch.subTitle.find(L':') != std::wstring::npos ? Text::Trim(ch.subTitle.substr(ch.subTitle.find(L':') + 1)) : ch.subTitle;
            TextStyle sub = Mono(12.f).SemiBold();
            sub.wrap = true;
            sub.valign = VAlign::Top;
            const float capH = g.Measure(L"Ag", Mono(9.f, true)).h, subH = g.Measure(subtitle, sub, col.w - 26.f).h;
            const Rect r = col.Next(1.f + 10.f + capH + 3.f + subH + 10.f + 1.f, 6.f);
            ButtonScope b(ui, ui.MakeId("chapter", ch.number), r,
                          { selected ? Rgba(0, 60, 50, 200) : Rgba(9, 20, 30, 180), selected ? Palette::Teal : Rgba(17, 34, 51, 140), 1.f, 4.f,
                            { 12.f, 10.f }, 0.f, r.h });
            g.Text(L"CAP\x00CDTULO " + std::to_wstring(ch.number), { b.content.x, b.content.y, b.content.w, capH }, Palette::Teal, Mono(9.f, true));
            g.Text(subtitle, { b.content.x, b.content.y + capH + 3.f, b.content.w, subH }, kDim, sub);
            if (b.Clicked()) SelectChapter(ch.number);
        }
        scroll.SetContentSize(col.Height());
    }
    // Run types
    {
        const Rect column{ body.x + 210.f + 16.f, body.y, body.R() - (body.x + 226.f), body.h };
        ScrollArea scroll(ui, ui.MakeId("runtypes", chapter), column, false, 48.f, ScrollArea::Bar::Classic);
        const float width = scroll.ContentWidth() - 4.f;
        float y = column.y;
        auto it = runTypes.find(chapter);
        if (it == runTypes.end()) {
            y += Status(ui, column.x, y, width, L"Cargando categor\x00EDas desde speedrun.com...", kDim);
            ui.KeepAnimatingThrottled();
        } else if (it->second.empty()) {
            y += Status(ui, column.x, y, width, L"No se pudieron cargar los datos de speedrun.com.", Rgba(204, 51, 51));
        } else {
            if (shownChapter != chapter) {
                shownChapter = chapter;
                cardsShownAt = std::max(ui.Time(), openedAt + 0.2);
            }
            for (size_t i = 0; i < it->second.size(); ++i) {
                StaggerItem in(ui, cardsShownAt, static_cast<int>(i), 0.f, 24.f, 0.07, 0.42);
                y += DrawCard(it->second[i], i, column.x, y, width, scroll.Offset()) + 12.f;
            }
        }
        scroll.SetContentSize(y - column.y);
    }
    if (!loadingRunners.empty() || !loadingChapters.empty()) ui.KeepAnimatingThrottled();
    PanelDecor(ui, panel, 8.f, Palette::Teal, openT, header.B());
    if (PanelEntranceRunning(openT)) ui.KeepAnimating();
    if (backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) Close();
}

} // namespace SpeedrunLauncher::Windows
