#pragma once
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main {

// "[ CONFIGURACIÓN ]": 620×640 panel with a 155 px sidebar of tabs. Each tab's content is
// drawn by its own file in Settings\Tabs\.
class SettingsOverlay : public Overlay {
public:
    enum Tab { General, Controls, LoadManip, Cores, Steam, Controller, Discord, LiveSplit, Overlays, IconTheme, TabCount };

    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Refresh() override { capture.Cancel(); }

    void Open(Tab tab = General);
    void Close();
    void SelectTab(Tab tab);

private:
    void DrawSidebar(UI::Ui& ui, const Rect& area);

    UI::Popup popup;
    Tab tab = General;
    double openedAt = -10.0;       // sidebar tabs slide in one by one after opening
    double tabChangedAt = -10.0;   // content rows slide in one by one after opening / a tab change
    int shownLoadManipPage = 0;    // a Load Manip sub-page change replays the rows too
    KeyCapture capture;
    int loadManipPage = 0;
    bool langOpen = false;
};

} // namespace SpeedrunLauncher::Main
