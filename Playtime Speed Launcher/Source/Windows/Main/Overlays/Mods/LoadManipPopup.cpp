// [ LOAD MANIP ] popup: UE4SS gate (Chapters 1, 4 and 5), add, or remove (keeping or also removing
// FullBright).
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"
#include "Windows/Main/Overlays/Mods/ModPopups.h"

namespace SpeedrunLauncher::Main {

namespace MI = Services::ModInstaller;

void ModPopups::OpenLoadManip(int chapter) {
    target = MI::Resolve(chapter);
    loadManipUe4ssThisSession = false;
    if (!target.win64) {
        ShowModMessage(main.Hwnd(), L"LOAD MANIP", L"No game path found for this chapter.\nPlease set up the game installation first.", false);
        return;
    }
    TakeSnapshot();
    loadManip.Open(true);
}

void ModPopups::DrawLoadManip(UI::Ui& ui, const Rect& screen) {
    const int chapter = target.chapter;
    const bool needsUe4ss = (chapter == 1 || chapter == 4 || chapter == 5) && !snap.ue4ss;
    const bool installed = !needsUe4ss && snap.loadManip;
    const MI::Target t = target;
    const auto close = [this] { loadManip.Close(true); };

    std::wstring question, warning;
    std::vector<ModButton> left, right;
    if (needsUe4ss) {
        question = L"UE4SS is required before adding\nLoad Manip files for Chapter " + std::to_wstring(chapter) + L".";
        if (chapter == 1) warning = L"\x26A0 Only compatible with Patch 1.3";
        right.push_back({ "lm-ue4ss", L"INSTALL UE4SS", false, 140.f, 12.f, 11.f, 10.f, !busy,
                          [this] { InstallUe4ssInPlace(loadManipUe4ssThisSession, L"LOAD MANIP"); } });
    } else if (installed) {
        question = L"Do you want to remove Load Manip files\nfrom this version?";
        left.push_back({ "lm-delete", L"DELETE", true, 100.f, 14.f, 12.f, 8.f, true, [this, t, close] {
                             close();
                             Run(L"LOAD MANIP", [t] { return MI::RemoveLoadManip(t, false); });
                         } });
        if (snap.fullBright) {
            left.push_back({ "lm-delete-all", L"DELETE ALL", true, 110.f, 14.f, 12.f, 0.f, true, [this, t, close] {
                                 close();
                                 Run(L"LOAD MANIP", [t] { return MI::RemoveLoadManip(t, true); });
                             } });
            warning = L"\x26A0 DELETE keeps FullBright installed.\nDELETE ALL removes FullBright too.";
        }
    } else {
        question = L"Do you want to add Load Manip files\nto this version?";
        if (chapter == 1) warning = L"\x26A0 Only compatible with Patch 1.3";
        right.push_back({ "lm-yes", L"YES", false, 110.f, 16.f, 12.f, 10.f, true, [this, t, close] {
                              close();
                              const auto install = [this, t](bool deleteForeign) {
                                  Run(L"LOAD MANIP", [t, deleteForeign] { return MI::InstallLoadManip(t, deleteForeign); });
                              };
                              // A UE4SS that isn't Load Manip's own build has to go first
                              const auto proceed = [this, t, install] {
                                  if (!loadManipUe4ssThisSession && MI::ForeignUe4ssBlocksLoadManip(t))
                                      ConfirmDeleteUe4ss(main.Hwnd(), L"LOAD MANIP", [install] { install(true); });
                                  else
                                      install(false);
                              };
                              if (MI::LegacyLoadManipPresent(t)) ConfirmDeleteLegacyLoadManip(main.Hwnd(), L"LOAD MANIP", proceed);
                              else proceed();
                          } });
    }
    right.push_back({ "lm-no", L"NO", true, 110.f, 16.f, 12.f, 0.f, true, close });
    DrawModPopup(ui, loadManip, screen, 460.f, L"[ LOAD MANIP ]", question, warning, left, right);
}

} // namespace SpeedrunLauncher::Main
