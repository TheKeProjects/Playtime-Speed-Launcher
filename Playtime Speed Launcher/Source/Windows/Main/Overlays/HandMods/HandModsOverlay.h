#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/GameFiles/HandModsService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Common/DiscordIdentity.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "✋ Hand Mods": chapter picker → which installed version → the chapter's community hand mods
// (info / preview / per-colour extras / install / uninstall) → install progress; plus a form to
// submit a new mod for review.
//   HandModsOverlay.cpp  frame, hub, version picker, navigation
//   HandModsList.cpp     mod list + its dialogs (info, preview, extras)
//   HandModsInstall.cpp  install screen (download, hand-colour conflicts, install)
//   HandModsSubmit.cpp   submission form
class HandModsOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void OpenHub();
    void OpenChapter(int chapter);

private:
    enum class Screen { Hub, Versions, Mods, Submit, Installing };

    // HandModsOverlay.cpp
    void DrawHub(UI::Ui& ui, UI::Column& col);
    void DrawVersions(UI::Ui& ui, UI::Column& col);
    void SelectVersion(const std::wstring& exePath);
    void ShowMessage(const std::wstring& message);

    // HandModsList.cpp
    void LoadMods();
    void DrawMods(UI::Ui& ui, UI::Column& col);
    void ShowInfo(const Services::HandMods::Mod& mod);
    void ShowPreview(const Services::HandMods::Mod& mod);
    void ShowExtras(const Services::HandMods::Mod& mod);
    void Uninstall(const Services::HandMods::Mod& mod);

    // HandModsInstall.cpp
    void StartInstall(const Services::HandMods::Mod& mod);
    void InstallDownloaded(const Services::HandMods::Mod& mod, const std::wstring& zipPath);
    void DrawInstalling(UI::Ui& ui, const Rect& body);

    // HandModsSubmit.cpp
    void OpenSubmit();
    void DrawSubmit(UI::Ui& ui, UI::Column& col);
    void Send();

    UI::Popup popup;
    Screen screen = Screen::Hub;
    int chapter = 0;
    std::optional<std::wstring> win64, paks;

    // Mod list
    std::map<int, std::vector<Services::HandMods::Mod>> cache;   // per chapter, this session
    std::optional<std::vector<Services::HandMods::Mod>> mods;
    bool loading = false;
    std::wstring loadError;
    std::set<std::wstring> busyButtons;                          // info / preview / extras waiting on a download
    int listRevision = 0;                                        // bumps after (un)installs (row state is re-read)

    // Install screen
    struct Install {
        std::wstring modName;
        float progress = 0.f;          // 0..100
        float animFrom = 0.f;
        double animStart = 0.0, animDuration = 0.0;
        std::wstring text;
        bool error = false;
        bool done = false;
    } install;

    // Submission form
    int submitChapter = 1;
    std::vector<std::wstring> submitColors, submitFiles;
    std::wstring submitName, submitStatus;
    bool submitOk = false, sending = false, infoOpen = false;
    DiscordIdentity discord;
};

} // namespace SpeedrunLauncher::Main
