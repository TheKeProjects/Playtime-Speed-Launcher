#pragma once
#include <optional>
#include <string>
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Common/DiscordIdentity.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ REPORT BUG ]", laid out like the Inazuma editor's reports tab: an animated banner, the form on
// the left (title, description, a screenshot drop zone and the send button) and on the right the
// Discord card and a checklist that ticks itself as the report gets ready. Sent to the team's
// Discord webhook. The info button on the Discord card opens the Discord info popup on top.
class BugReportOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open();

private:
    void DrawForm(UI::Ui& ui, const Rect& r);
    float DrawDiscord(UI::Ui& ui, const Rect& r, bool draw);   // returns the card's height
    bool Ready() const;   // Discord connected, a title and a description of 20+ characters
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
