#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Core/Async.h"
#include "Models/Chapter.h"

namespace SpeedrunLauncher::UI {
class Window;
}

namespace SpeedrunLauncher::Main {

// Installing a historic build: the AUTO path (SteamCMD download_depot with the user's Steam login
// and a live log) or the MANUAL path (instructions for the Steam console), then moving the
// downloaded depot into a folder of the user's choice and registering it as an installation.
class PresetInstaller {
public:
    struct Download {
        Async::Cancel cancel;
        std::vector<std::wstring> lines;   // [0] is the "[ mm:ss ]  in progress" line
        double startedAt = 0.0;            // Async::Now()
        bool finished = false;
        bool phoneCheck = false;           // signed in with a password: Steam may ask the phone to approve it
        bool phoneWaiting = false;         // told to approve it on the phone; until the files start coming in
    };
    // The download finished but its files weren't where they should be
    struct NotFound {
        Models::ChapterPreset preset;
        int chapterNumber = 0;
        std::optional<std::wstring> steamCmd;
    };
    // Downloaded and found, waiting for the user to name it and pick its folder
    struct Ready {
        Models::ChapterPreset preset;
        int chapterNumber = 0;
        std::wstring depotPath;
    };

    explicit PresetInstaller(const UI::Window& ownerWindow) : window(ownerWindow) {}
    ~PresetInstaller() { CancelAll(); }

    // "Install" on a preset row: asks AUTO / MANUAL.
    void Install(const Models::ChapterPreset& preset, int chapterNumber);
    // The store's product page runs the steps itself (method, Steam account, manual steps) and
    // then hands over here:
    //   StartAutoWith       AUTO with that account (finds / offers to get SteamCMD first)
    //   ManualDepotFolder   where the Steam console will put the files (created if missing)
    //   OpenManualConsole   command on the clipboard, that folder and then the Steam console opened
    //   ManualDownloaded    the user says the console finished: move and register the files
    void StartAutoWith(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& user, const std::optional<std::wstring>& password);
    static std::wstring ManualDepotFolder(const Models::ChapterPreset& preset);
    void OpenManualConsole(const Models::ChapterPreset& preset);
    void ManualDownloaded(const Models::ChapterPreset& preset, int chapterNumber) { LocateDepot(preset, chapterNumber, std::nullopt); }
    bool IsDownloading(const std::wstring& manifestId) const { return downloads.count(manifestId) != 0; }
    // Last lines of a running download's log (with the elapsed time refreshed).
    std::vector<std::wstring> LogTail(const std::wstring& manifestId, size_t count) const;
    void CancelAll();
    // First run without steamcmd.exe: offer to download / locate it. `then` gets the path (or nothing).
    void AcquireSteamCmd(std::function<void(std::optional<std::wstring>)> then = {});

    // After the download (the store's product page shows these steps)
    bool PhoneCheck(const std::wstring& manifestId) const;
    void DismissPhoneCheck(const std::wstring& manifestId);   // → waiting for the approval on the phone
    // Waiting for the approval in the Steam app (ends by itself when the files start downloading)
    bool PhoneWaiting(const std::wstring& manifestId) const;
    void SkipPhoneWaiting(const std::wstring& manifestId);
    const Ready* ReadyFor(const std::wstring& manifestId) const;
    // Files not found: pick their folder by hand, look again, or give up
    bool NotFoundFor(const std::wstring& manifestId) const { return notFound.count(manifestId) != 0; }
    void UseFolder(const std::wstring& manifestId, const std::wstring& folder);
    void RetryLocate(const std::wstring& manifestId);
    void DismissNotFound(const std::wstring& manifestId) { notFound.erase(manifestId); }
    void Later(const std::wstring& manifestId) { ready.erase(manifestId); }   // the files stay where they were downloaded
    // Moves the files into `folder`\<name> and registers the installation
    void Finish(const std::wstring& manifestId, std::wstring name, const std::optional<std::wstring>& icon, const std::wstring& folder);
    std::optional<std::wstring> MovingStatus(const std::wstring& manifestId) const;   // while moving

    std::function<void()> onInstalled;   // a new installation was registered
    std::function<void(int chapterNumber, const std::wstring& manifestId)> onReady;   // files downloaded, waiting for name / folder
    std::function<void(int chapterNumber)> onFinished;                                // moved and registered
    std::function<void(int chapterNumber, const std::wstring& manifestId)> onNotFound;   // downloaded files not found

private:
    void StartAuto(const Models::ChapterPreset& preset, int chapterNumber);
    void StartManual(const Models::ChapterPreset& preset, int chapterNumber);
    void PromptCredentials(const Models::ChapterPreset& preset, std::function<void(std::wstring user, std::optional<std::wstring> pass)> then);
    void RunSteamCmd(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& steamCmd, const std::wstring& user,
                     const std::optional<std::wstring>& password);
    void LocateDepot(const Models::ChapterPreset& preset, int chapterNumber, const std::optional<std::wstring>& steamCmd);
    void SetReady(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& depotPath);
    void ShowError(const std::wstring& message);

    HWND Owner() const;
    const UI::Window& window;
    std::map<std::wstring, std::shared_ptr<Download>> downloads;
    std::map<std::wstring, Ready> ready;
    std::map<std::wstring, NotFound> notFound;
    std::map<std::wstring, std::shared_ptr<std::wstring>> moving;   // status text per manifest
};

// "Parche" → localized "Patch" / "Parche"
std::wstring TranslatePresetName(const std::wstring& name);

} // namespace SpeedrunLauncher::Main
