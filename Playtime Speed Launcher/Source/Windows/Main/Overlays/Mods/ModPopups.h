#pragma once
#include <functional>
#include <optional>
#include <string>
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
    // Same, but `popup` stays open on an animated working screen (`label`, e.g. "Installing Load
    // Manip…") and shows the result in place. With `onSuccess` a success goes straight back to the
    // popup's question in its new state instead (the INSTALL UE4SS gate).
    // `glyph` floats in the ring (install / delete).
    void RunInPopup(UI::Popup& popup, std::wstring label, wchar_t glyph, std::function<Services::ModInstaller::Result()> work,
                    std::function<void()> onSuccess = {});
    void InstallUe4ssInPlace(bool& installedThisSession, UI::Popup& popup);
    // Draws `popup`'s working / result screen when it has a job; returns false when it hasn't.
    bool DrawJob(UI::Ui& ui, UI::Popup& popup, const Rect& screen, float width, const std::wstring& title);

    struct Job {
        UI::Popup* popup = nullptr;
        std::wstring label;
        wchar_t glyph = 0;
        bool finished = false, ok = false;
        std::wstring head, body;   // result message: first line, the rest
        double startedAt = -1.0, finishedAt = -1.0;   // stamped by the first frame that shows them
    };
    std::optional<Job> job;

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
