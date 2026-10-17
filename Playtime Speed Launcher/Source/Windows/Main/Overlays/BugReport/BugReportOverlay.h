#pragma once
#include <optional>
#include <string>
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Common/DiscordIdentity.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ REPORT BUG ]": Discord identity, title, description and an optional screenshot, sent to the
// team's Discord webhook. The "?" next to Connect opens the Discord info popup on top.
class BugReportOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open();

private:
    float DrawForm(UI::Ui& ui, const Rect& body);   // returns the content height
    void Send();

    UI::Popup popup;
    DiscordIdentity discord;
    std::wstring title, description;
    std::optional<std::wstring> image;
    std::wstring status;
    bool statusOk = false;
    bool sending = false;
    bool infoOpen = false;
    double openedAt = -10.0;   // form blocks slide in one by one
};

} // namespace SpeedrunLauncher::Main
