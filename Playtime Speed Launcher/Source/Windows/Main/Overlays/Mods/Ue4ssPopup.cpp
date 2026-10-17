// [ UE4SS ] popup: add / remove the standalone UE4SS build, or replace a Load Manip / FullBright
// owned one with it.
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"
#include "Windows/Main/Overlays/Mods/ModPopups.h"

namespace SpeedrunLauncher::Main {

namespace MI = Services::ModInstaller;

void ModPopups::OpenUe4ss(int chapter) {
    target = MI::Resolve(chapter);
    TakeSnapshot();
    ue4ss.Open(true);
}

void ModPopups::DrawUe4ss(UI::Ui& ui, const Rect& screen) {
    const bool viaLoadManip = snap.loadManipMarker;
    const bool viaFullBright = snap.fullBright;
    const bool ownedByMod = viaLoadManip || viaFullBright;
    const bool installed = snap.standaloneUe4ss;
    const MI::Target t = target;

    std::wstring question;
    std::vector<ModButton> left, right;
    const auto close = [this] { ue4ss.Close(true); };
    if (ownedByMod) {
        question = viaLoadManip && viaFullBright ? L"Load Manip, FullBright and their UE4SS files must be\nremoved before installing the original UE4SS."
                   : viaLoadManip               ? L"Load Manip and its UE4SS files must be\nremoved before installing the original UE4SS."
                                                : L"FullBright and its UE4SS files must be\nremoved before installing the original UE4SS.";
        left.push_back({ "ue4ss-delete", L"DELETE", true, 110.f, 16.f, 12.f, 10.f, true, [this, t, close] {
                             close();
                             Run(L"UE4SS", [t] { return MI::ReplaceModUe4ssWithStandalone(t); });
                         } });
    } else if (installed) {
        question = L"Do you want to remove UE4SS\nfrom this version?";
        left.push_back({ "ue4ss-delete", L"DELETE", true, 110.f, 16.f, 12.f, 10.f, true, [this, t, close] {
                             close();
                             Run(L"UE4SS", [t] { return MI::RemoveUe4ss(t); });
                         } });
    } else {
        question = L"Do you want to add UE4SS\nto this version?";
        right.push_back({ "ue4ss-yes", L"YES", false, 110.f, 16.f, 12.f, 10.f, true, [this, t, close] {
                              close();
                              Run(L"UE4SS", [t] { return MI::InstallUe4ss(t); });
                          } });
    }
    right.push_back({ "ue4ss-no", L"NO", true, 110.f, 16.f, 12.f, 0.f, true, close });
    DrawModPopup(ui, ue4ss, screen, 420.f, L"[ UE4SS ]", question, L"", left, right);
}

} // namespace SpeedrunLauncher::Main
