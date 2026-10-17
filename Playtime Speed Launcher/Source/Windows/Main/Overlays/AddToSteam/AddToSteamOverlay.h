#pragma once
#include <string>
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ ADD TO STEAM ]": pick the library icon, then hand over to the elevated
// --steam-shortcut-helper process (it closes Steam, edits shortcuts.vdf and restarts Steam)
// and exit, so this process can't be the one Steam kills while shutting down.
class AddToSteamOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open();

private:
    void Confirm();

    UI::Popup popup;
    std::wstring icon;
    bool steamRunning = false;
};

} // namespace SpeedrunLauncher::Main
