#include "Windows/Main/Overlays/Mods/ModPopups.h"
#include <windows.h>
#include "Core/Async.h"
#include "UI/Style.h"
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

void ModPopups::RunInPopup(UI::Popup& popup, std::wstring label, wchar_t glyph, std::function<MI::Result()> work, std::function<void()> onSuccess) {
    if (busy) return;
    busy = true;
    job = Job{ &popup, std::move(label), glyph };
    // Fast operations still show the screen for a moment, so it doesn't just flash
    Async::Run([work = std::move(work)] {
        const ULONGLONG begin = GetTickCount64();
        MI::Result result = work();
        const ULONGLONG spent = GetTickCount64() - begin;
        if (spent < 1100) Sleep(static_cast<DWORD>(1100 - spent));
        return result;
    }, [this, onSuccess = std::move(onSuccess)](MI::Result result) {
        busy = false;
        main.RefreshInfo();   // card buttons + the Settings key rows read the new state
        if (!job) return;
        if (result.ok && onSuccess) {
            job.reset();
            onSuccess();
            TakeSnapshot();
            return;
        }
        const size_t nl = result.message.find(L'\n');
        job->head = nl == std::wstring::npos ? result.message : result.message.substr(0, nl);
        job->body = nl == std::wstring::npos ? L"" : result.message.substr(nl + 1);
        if (job->head.empty()) job->head = result.ok ? L"Done!" : L"Something went wrong.";
        job->ok = result.ok;
        job->finished = true;
        MessageBeep(result.ok ? MB_ICONINFORMATION : MB_ICONERROR);
    });
}

bool ModPopups::DrawJob(UI::Ui& ui, UI::Popup& popup, const Rect& screen, float width, const std::wstring& title) {
    if (!job || job->popup != &popup) return false;
    Job& j = *job;
    if (j.startedAt < 0.0) j.startedAt = ui.Time();
    if (j.finished && j.finishedAt < 0.0) j.finishedAt = ui.Time();
    ModWorkState state{ j.label, j.glyph, j.startedAt, j.finished, j.ok, j.head, j.body, j.finishedAt };
    if (DrawModWorking(ui, popup, screen, width, title, state)) popup.Close(true);   // the result fades out with the popup
    return true;
}

// The popups' "INSTALL UE4SS" gate: install the generic build on the working screen, then go back
// to the popup in its next state.
void ModPopups::InstallUe4ssInPlace(bool& installedThisSession, UI::Popup& popup) {
    if (busy) return;
    const MI::Target t = target;
    bool* flag = &installedThisSession;
    RunInPopup(popup, L"Installing UE4SS\x2026", UI::Glyphs::Install, [t] { return MI::InstallUe4ss(t); }, [flag] { *flag = true; });
}

void ModPopups::Draw(UI::Ui& ui, const Rect& screen) {
    if (job && job->finished && !job->popup->IsOpen() && !job->popup->Visible()) job.reset();   // its popup has finished closing
    if (ue4ss.Visible()) DrawUe4ss(ui, screen);
    if (loadManip.Visible()) DrawLoadManip(ui, screen);
    if (fullBright.Visible()) DrawFullBright(ui, screen);
}

} // namespace SpeedrunLauncher::Main
