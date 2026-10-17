#pragma once
#include <functional>
#include "Services/GameFiles/ModInstaller.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// The three yes/no popups of the chapter-card mod buttons: [ UE4SS ], [ LOAD MANIP ] and
// [ FULLBRIGHT ]. Each file in Mods\ draws one of them; the file operations live in
// Services\GameFiles\ModInstaller.
class ModPopups : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return ue4ss.Visible() || loadManip.Visible() || fullBright.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void OpenUe4ss(int chapter);
    void OpenLoadManip(int chapter);
    void OpenFullBright(int chapter);

private:
    void DrawUe4ss(UI::Ui& ui, const Rect& screen);
    void DrawLoadManip(UI::Ui& ui, const Rect& screen);
    void DrawFullBright(UI::Ui& ui, const Rect& screen);

    // Runs a ModInstaller operation off the UI thread, then shows its message and refreshes.
    void Run(const wchar_t* title, std::function<Services::ModInstaller::Result()> work);
    void InstallUe4ssInPlace(bool& installedThisSession, const wchar_t* title);

    // File state captured when a popup opens (and after its INSTALL UE4SS step), so a popup
    // fading out after an action keeps showing what the user clicked on.
    struct Snapshot {
        bool ue4ss = false;            // any UE4SS build present
        bool standaloneUe4ss = false;
        bool loadManipMarker = false;
        bool loadManip = false;
        bool fullBright = false;
    };
    void TakeSnapshot();

    Services::ModInstaller::Target target;
    Snapshot snap;
    UI::Popup ue4ss, loadManip, fullBright;
    bool busy = false;
    bool loadManipUe4ssThisSession = false, fullBrightUe4ssThisSession = false;
};

} // namespace SpeedrunLauncher::Main
