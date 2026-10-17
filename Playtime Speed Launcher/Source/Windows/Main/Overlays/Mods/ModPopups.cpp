#include "Windows/Main/Overlays/Mods/ModPopups.h"
#include "Core/Async.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"

namespace SpeedrunLauncher::Main {

namespace MI = Services::ModInstaller;

void ModPopups::TakeSnapshot() {
    snap.ue4ss = MI::Ue4ssPresent(target);
    snap.standaloneUe4ss = MI::StandaloneUe4ssInstalled(target);
    snap.loadManipMarker = MI::LoadManipMarker(target);
    snap.loadManip = MI::LoadManipInstalled(target);
    snap.fullBright = MI::FullBrightInstalled(target);
}

void ModPopups::Run(const wchar_t* title, std::function<MI::Result()> work) {
    const std::wstring caption = title;
    Async::Run(std::move(work), [this, caption](MI::Result result) {
        main.RefreshInfo();   // card buttons + the Settings key rows read the new state
        ShowModMessage(main.Hwnd(), caption, result.message, result.ok);
    });
}

// The popups' "INSTALL UE4SS" gate: install the generic build, then stay open in the next state.
void ModPopups::InstallUe4ssInPlace(bool& installedThisSession, const wchar_t* title) {
    if (busy) return;
    busy = true;
    const MI::Target t = target;
    const std::wstring caption = title;
    bool* flag = &installedThisSession;
    Async::Run([t] { return MI::InstallUe4ss(t); }, [this, flag, caption](MI::Result result) {
        busy = false;
        main.RefreshInfo();
        if (result.ok) {
            *flag = true;
            TakeSnapshot();
            return;
        }
        ue4ss.Close(true);
        loadManip.Close(true);
        fullBright.Close(true);
        ShowModMessage(main.Hwnd(), caption, result.message, false);
    });
}

void ModPopups::Draw(UI::Ui& ui, const Rect& screen) {
    if (ue4ss.Visible()) DrawUe4ss(ui, screen);
    if (loadManip.Visible()) DrawLoadManip(ui, screen);
    if (fullBright.Visible()) DrawFullBright(ui, screen);
}

} // namespace SpeedrunLauncher::Main
