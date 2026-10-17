#include "Windows/Main/Settings/SettingsOverlay.h"
#include <array>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

struct TabInfo {
    const char* key;
    void (*draw)(SettingsContext&);
};

// Sidebar order (same as the old XAML)
constexpr std::array<TabInfo, SettingsOverlay::TabCount> kTabs{ {
    { "settings_tab_general", SettingsTabs::General },
    { "settings_tab_controls", SettingsTabs::Controls },
    { "settings_tab_loadmanip", SettingsTabs::LoadManip },
    { "settings_tab_cores", SettingsTabs::Cores },
    { "settings_tab_steam", SettingsTabs::Steam },
    { "settings_tab_controller", SettingsTabs::Controller },
    { "settings_tab_discord", SettingsTabs::Discord },
    { "settings_tab_updates", SettingsTabs::Updates },
    { "settings_tab_livesplit", SettingsTabs::LiveSplit },
    { "settings_tab_overlays", SettingsTabs::Overlays },
    { "settings_tab_icontheme", SettingsTabs::IconTheme },
} };

constexpr float kSidebarWidth = 155.f;
constexpr Color kAlertRed = Rgba(204, 34, 0);

} // namespace

void SettingsOverlay::Open(Tab t) {
    popup.Open();
    SelectTab(t);
    openedAt = Async::Now();
    tabChangedAt = openedAt + 0.12;   // the content follows right behind the sidebar
}

void SettingsOverlay::Close() {
    capture.Cancel();
    langOpen = false;
    popup.Close();
}

void SettingsOverlay::SelectTab(Tab t) {
    capture.Cancel();
    langOpen = false;
    if (t != tab) tabChangedAt = Async::Now();
    tab = t;
    if (t == LoadManip) loadManipPage = 0;
}

void SettingsOverlay::DrawSidebar(Ui& ui, const Rect& area) {
    auto& g = ui.g;
    g.FillRect(area, Palette::HeaderBg);
    g.FillRect({ area.R() - 1.f, area.y, 1.f, area.h }, Palette::Divider);
    float y = area.y + 14.f;
    for (int i = 0; i < TabCount; ++i) {
        const Rect r{ area.x, y, area.w - 1.f, 36.f };
        y += 36.f;
        const bool selected = i == tab;
        StaggerItem reveal(ui, openedAt, i, -34.f, 0.f, 0.03, 0.34);   // tabs slide in from the left one by one
        ButtonScope b(ui, ui.MakeId("settings-tab", i), r, Styles::Flat());
        if (selected) g.FillRect({ r.x, r.y, 3.f, r.h }, Palette::Teal);
        const std::wstring text = Loc::Get(kTabs[static_cast<size_t>(i)].key);
        const TextStyle style = Mono(11.f, true);
        Rect textArea = r.CutLeft(17.f);
        if (i == Updates && main.UpdateAlert()) {
            // Red text + breathing red border while an update is waiting
            const float t = Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 0.8));
            const Color border = Lerp(kAlertRed, Rgba(80, 10, 0), t);
            const float w = g.Measure(text, style).w + 8.f + 2.f, h = g.Measure(text, style).h + 4.f + 2.f;
            const Rect box{ textArea.x, r.CY() - h * 0.5f, w, h };
            g.Border(box, 3.f, Colors::Transparent, border, 1.f);
            g.Text(text, box.Inset(5.f, 3.f), selected ? Palette::Teal : kAlertRed, style);
            ui.KeepAnimatingThrottled();
        } else {
            g.Text(text, i == Updates ? textArea.CutLeft(5.f) : textArea, selected ? Palette::Teal : Palette::Dim, style);
        }
        if (b.Clicked()) SelectTab(static_cast<Tab>(i));
    }
}

void SettingsOverlay::Draw(Ui& ui, const Rect& screen) {
    PanelStyle style;
    style.title = Loc::Get("settings_title");
    OverlayFrame frame(ui, ui.MakeId("settings"), screen, 620.f, 640.f, style, popup.Opacity(), popup.Scale());
    capture.Poll(ui);

    const Rect sidebar{ frame.body.x, frame.body.y, kSidebarWidth, frame.body.h };
    const Rect content = frame.body.CutLeft(kSidebarWidth);
    if (loadManipPage != shownLoadManipPage) {
        shownLoadManipPage = loadManipPage;
        tabChangedAt = Async::Now();
    }
    {
        // The rows of the tab slide in from the right one after another (see SettingsContext::Reveal)
        ScrollArea scroll(ui, ui.MakeId("settings-scroll", tab), content, false, 48.f, ScrollArea::Bar::Classic);
        SettingsContext c{ ui, main, capture, Column(content.x + 24.f, content.y + 18.f, scroll.ContentWidth() - 48.f), loadManipPage, langOpen };
        c.revealStart = tabChangedAt;
        kTabs[static_cast<size_t>(tab)].draw(c);
        scroll.SetContentSize(c.col.Height() + 36.f);
    }
    DrawSidebar(ui, sidebar);

    // Footer: VOLVER on the right
    const std::wstring back = Loc::Get("back");
    const Rect btn{ frame.footer.R() - 14.f - 120.f, frame.footer.CY() - 18.f, 120.f, 36.f };
    if (TextButton(ui, ui.MakeId("settings-close"), btn, back, Mono(11.f, true).Center(), Palette::Dim, Styles::Footer())) Close();
}

} // namespace SpeedrunLauncher::Main
