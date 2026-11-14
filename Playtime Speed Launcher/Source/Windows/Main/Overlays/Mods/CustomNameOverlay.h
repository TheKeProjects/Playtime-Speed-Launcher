#pragma once
#include <optional>
#include <string>
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ CUSTOM NAME ]" (Escape From Playtime old versions): the in-game name instead of the
// "<PC NAME>-<CODE>" of the old builds. The name and its look (colour, size, shadow / outline /
// bold, animation) are shared by every version, with a live preview; the footer turns the mod on
// or off in the version PLAY starts, like Load Manip. Changes are copied to every version that has
// the mod (the game picks them up within 2 seconds, even while it's running).
class CustomNameOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void Open();   // for the selected multiplayer game's active version

private:
    void DrawPreview(UI::Ui& ui, const Rect& r);
    void Changed(UI::Ui& ui);   // a setting changed: saved and synced shortly after
    void Flush();
    void Toggle();               // add / remove the mod in this version

    UI::Popup popup;
    std::optional<std::wstring> win64;
    std::wstring versionName;
    bool installed = false;
    bool busy = false;
    bool dirty = false;
    double dirtyAt = 0.0;
    std::wstring message;        // last add / remove result
    bool messageOk = true;
};

} // namespace SpeedrunLauncher::Main
