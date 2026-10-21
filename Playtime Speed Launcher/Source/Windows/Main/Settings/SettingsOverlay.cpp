#include "Windows/Main/Settings/SettingsOverlay.h"
#include <array>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
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
    { "settings_tab_livesplit", SettingsTabs::LiveSplit },
    { "settings_tab_overlays", SettingsTabs::Overlays },
    { "settings_tab_icontheme", SettingsTabs::IconTheme },
} };

constexpr float kSidebarWidth = 155.f;

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
    // Selection: a glowing bar and a soft highlight that slide to the selected tab
    const float top = area.y + 14.f;
    {
        const float selY = ui.Animate(ui.MakeId("settings-tab-sel"), top + 36.f * static_cast<float>(tab), 16.f);
        const Rect sel{ area.x, selY, area.w - 1.f, 36.f };
        g.FillLinear(sel, 0.f, { { 0.f, Palette::Teal.WithAlpha(0.14f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        g.GlowRound({ sel.x, sel.y + 4.f, 3.f, sel.h - 8.f }, 1.5f, Palette::Teal, 10.f, 0.6f);
        g.FillRound({ sel.x, sel.y + 4.f, 3.f, sel.h - 8.f }, 1.5f, Palette::Teal);
    }
    float y = top;
    for (int i = 0; i < TabCount; ++i) {
        const Rect r{ area.x, y, area.w - 1.f, 36.f };
        y += 36.f;
        const bool selected = i == tab;
        StaggerItem reveal(ui, openedAt, i, -34.f, 0.f, 0.03, 0.34);   // tabs slide in from the left one by one
        const Id tabId = ui.MakeId("settings-tab", i);
        const float hover = ui.Animate(HashId(tabId, 0x4F56), ui.Hot(tabId) && !selected ? 1.f : 0.f, 16.f, 0.f);
        ButtonScope b(ui, tabId, r, Styles::Flat());
        if (hover > 0.01f) g.FillRect(r, Palette::Teal.WithAlpha(0.05f * hover));
        const std::wstring text = Loc::Get(kTabs[static_cast<size_t>(i)].key);
        const TextStyle style = Mono(11.f, true);
        Rect textArea = r.CutLeft(17.f + 4.f * hover + (selected ? 3.f : 0.f));
        g.Text(text, textArea, selected ? Palette::Teal : Lerp(Palette::Dim, Palette::Label, hover), style);
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
    const float bw = MenuKit::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
    const Rect btn{ frame.footer.R() - 14.f - bw, frame.footer.CY() - 18.f, bw, 36.f };
    if (MenuKit::FooterButton(ui, ui.MakeId("settings-close"), btn, back, MenuKit::Kind::Secondary, true, Glyphs::Back)) Close();
}

} // namespace SpeedrunLauncher::Main
