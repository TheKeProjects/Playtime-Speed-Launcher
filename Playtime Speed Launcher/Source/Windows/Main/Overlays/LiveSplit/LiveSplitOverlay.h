#pragma once
#include <optional>
#include "Services/LiveSplit/LiveSplitService.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ LIVESPLIT ]": installed / latest version, install path and one action button
// (Install / Update / Launch) with download progress.
class LiveSplitOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void Open() { popup.Open(); }
    void StartupCheck();
    // Steps to enable the LiveSplit Server (shown when Discord's LiveSplit line is turned on).
    void ShowTcpNotice();
    const std::optional<Services::LiveSplit::Info>& Info() const { return info; }

private:
    void Act();

    UI::Popup popup;
    std::optional<Services::LiveSplit::Info> info;
    bool downloading = false;
    int percent = 0;
};

} // namespace SpeedrunLauncher::Main
