// [ FULLBRIGHT ] popup (Chapters 1, 4 and 5): UE4SS gate, add (alone or on top of Load Manip), or
// remove (restoring or also removing Load Manip).
#include "Services/GameFiles/LoadManipFiles.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"
#include "Windows/Main/Overlays/Mods/ModPopups.h"

namespace SpeedrunLauncher::Main {

namespace MI = Services::ModInstaller;

void ModPopups::OpenFullBright(int chapter) {
    target = MI::Resolve(chapter);
    fullBrightUe4ssThisSession = false;
    if (!target.win64) {
        ShowModMessage(main.Hwnd(), L"FULLBRIGHT", L"No game path found for this chapter.\nPlease set up the game installation first.", false);
        return;
    }
    TakeSnapshot();
    fullBright.Open(true);
}

void ModPopups::DrawFullBright(UI::Ui& ui, const Rect& screen) {
    const int chapter = target.chapter;
    const bool needsUe4ss = !snap.ue4ss;
    const bool installed = !needsUe4ss && snap.fullBright;
    const MI::Target t = target;
    const auto close = [this] { fullBright.Close(true); };

    std::wstring question, warning;
    std::vector<ModButton> left, right;
    if (needsUe4ss) {
        question = L"UE4SS is required before adding\nFullBright files for Chapter " + std::to_wstring(chapter) + L".";
        if (chapter == 1) warning = L"\x26A0 Only compatible with Patch 1.3";
        right.push_back({ "fb-ue4ss", L"INSTALL UE4SS", false, 140.f, 12.f, 11.f, 8.f, !busy,
                          [this] { InstallUe4ssInPlace(fullBrightUe4ssThisSession, L"FULLBRIGHT"); } });
    } else if (installed) {
        question = L"Do you want to remove FullBright files\nfrom this version?";
        left.push_back({ "fb-delete", L"DELETE", true, 100.f, 14.f, 12.f, 8.f, true, [this, t, close] {
                             close();
                             Run(L"FULLBRIGHT", [t] { return MI::RemoveFullBright(t, false); });
                         } });
        if (snap.loadManipMarker) {
            left.push_back({ "fb-delete-all", L"DELETE ALL", true, 110.f, 14.f, 12.f, 0.f, true, [this, t, close] {
                                 close();
                                 Run(L"FULLBRIGHT", [t] { return MI::RemoveFullBright(t, true); });
                             } });
            warning = L"\x26A0 DELETE keeps Load Manip installed (restored automatically).\nDELETE ALL removes Load Manip too.";
        }
    } else {
        question = L"Do you want to add FullBright files\nto this version?";
        if (chapter == 1) warning = L"\x26A0 Only compatible with Patch 1.3";
        const auto install = [this, t](bool withLoadManip) {
            const bool sessionUe4ss = fullBrightUe4ssThisSession;
            const auto run = [this, t, withLoadManip](bool deleteForeign) {
                Run(L"FULLBRIGHT", [t, withLoadManip, deleteForeign] { return MI::InstallFullBright(t, withLoadManip, deleteForeign); });
            };
            // A standalone UE4SS (not Load Manip's, not FullBright's) has to be deleted first
            bool blocked = !sessionUe4ss && MI::ForeignUe4ssBlocksFullBright(t);
            if (withLoadManip) blocked = blocked && !MI::LoadManipInstalled(t);
            const auto proceed = [this, blocked, run] {
                if (blocked) ConfirmDeleteUe4ss(main.Hwnd(), L"FULLBRIGHT", [run] { run(true); });
                else run(false);
            };
            if (MI::LegacyLoadManipPresent(t)) ConfirmDeleteLegacyLoadManip(main.Hwnd(), L"FULLBRIGHT", proceed);
            else proceed();
        };
        right.push_back({ "fb-only", L"ONLY FULLBRIGHT", false, 130.f, 12.f, 11.f, 8.f, true, [close, install] {
                              close();
                              install(false);
                          } });
        right.push_back({ "fb-yes", L"+ LOAD MANIP", false, 110.f, 12.f, 11.f, 8.f, true, [close, install] {
                              close();
                              install(true);
                          } });
    }
    right.push_back({ "fb-no", L"NO", true, 80.f, 14.f, 12.f, 0.f, true, close });
    DrawModPopup(ui, fullBright, screen, 470.f, L"[ FULLBRIGHT ]", question, warning, left, right);
}

} // namespace SpeedrunLauncher::Main
