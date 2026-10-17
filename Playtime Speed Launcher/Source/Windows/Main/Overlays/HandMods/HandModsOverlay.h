#pragma once
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Graphics/Images.h"
#include "Services/GameFiles/HandModsService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Common/DiscordIdentity.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// A mod's hand picture, decoded off the UI thread and uploaded once.
struct HandPreview {
    std::wstring color;
    ImagePtr image;
};

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
    // Switches screen with a slide (forward → from the right, back → from the left); the new
    // screen's rows cascade in again.
    void GoTo(Screen next, int direction = 1);
    void RefreshVersions();   // installed versions of `chapter` (read once per visit, not every frame)
    void DrawSteps(UI::Ui& ui, const Rect& r);
    void DrawHub(UI::Ui& ui, UI::Column& col);
    void DrawVersions(UI::Ui& ui, UI::Column& col);
    void SelectVersion(const std::wstring& exePath);
    void ShowMessage(const std::wstring& message);

    // HandModsList.cpp
    void LoadMods();
    void DrawMods(UI::Ui& ui, UI::Column& col);
    void ShowInfo(const Services::HandMods::Mod& mod);
    void ShowPreview(const Services::HandMods::Mod& mod);
    void DrawPreview(UI::Ui& ui, const Rect& screen);   // the preview viewer, over the whole window
    void SelectPreview(int index);
    void ClosePreview();
    const std::vector<bool>& InstalledFlags();          // per mod; the files are checked again only after (un)installs
    void ShowExtras(const Services::HandMods::Mod& mod);
    // HandModsExtras.cpp: the "Individual Hands" panel, over the whole window
    void DrawExtras(UI::Ui& ui, const Rect& screen);
    void CloseExtras();
    void InstallExtra(int index);
    void UninstallExtra(int index);
    void FinishExtra(int generation, int index, bool ok, const std::wstring& message);
    // HandModsConflict.cpp: "installing X replaces the Green hand; Y will be removed first" — asked
    // inside the menu; `done(true)` to go ahead
    void AskConflict(const std::wstring& incoming, const std::vector<std::wstring>& hands, const std::vector<std::wstring>& removed,
                     std::function<void(bool)> done);
    void DrawConflict(UI::Ui& ui, const Rect& screen);
    void AnswerConflict(bool proceed);
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
    double screenChangedAt = -10.0;   // Async::Now() of the last GoTo (drives the slide + cascades)
    int screenDirection = 1;
    int chapter = 0;
    std::optional<std::wstring> win64, paks;

    // Mod list
    std::map<int, std::vector<Services::HandMods::Mod>> cache;   // per chapter, this session
    std::optional<std::vector<Services::HandMods::Mod>> mods;
    bool loading = false;
    std::wstring loadError;
    std::set<std::wstring> busyButtons;                          // info / preview / extras waiting on a download
    int listRevision = 0;                                        // bumps after (un)installs (row state is re-read)
    std::vector<bool> installed;                                 // InstalledFlags() cache, valid for:
    int installedRevision = -1;
    const void* installedList = nullptr;

    // Version picker rows (RefreshVersions)
    struct VersionRow {
        std::wstring name, exe;
        bool automatic = false;
    };
    std::vector<VersionRow> versionRows;

    // Preview viewer (HandModsList.cpp)
    struct Viewer {
        bool open = false;
        Services::HandMods::Mod mod;
        std::vector<HandPreview> images;
        int selected = 0, previous = -1, direction = 1;
        double openedAt = 0.0, closingAt = -1.0, switchedAt = -10.0;
    } viewer;

    // Individual Hands panel (HandModsExtras.cpp)
    struct Extras {
        bool open = false;
        int generation = 0;                       // results of a closed panel's installs are ignored
        Services::HandMods::Mod mod;
        std::wstring zip;                         // the parent zip (extras are extracted from it)
        std::vector<Services::HandMods::Extra> extras;
        std::vector<HandPreview> previews;
        std::vector<bool> installed;              // re-read only after an (un)install
        int revision = -1;
        int busy = -1;                            // extra being installed / removed
        std::vector<double> doneAt;               // per extra: success flash
        std::wstring message;                     // inline note at the bottom ("Blue installed")
        bool messageError = false;
        double messageAt = -10.0;
        double openedAt = 0.0, closingAt = -1.0;
    } extrasPanel;

    // Hand conflict question (HandModsConflict.cpp)
    struct Conflict {
        bool open = false;
        std::wstring incoming;                    // what's being installed
        std::vector<std::wstring> hands;          // the hand colours it changes
        std::vector<std::wstring> removed;        // installed mods that go first
        std::function<void(bool)> done;
        bool answer = false;
        double openedAt = 0.0, closingAt = -1.0;
    } conflict;

    // Install screen
    struct Install {
        std::wstring modName;
        float progress = 0.f;          // 0..100
        float animFrom = 0.f;
        double animStart = 0.0, animDuration = 0.0;
        std::wstring text;
        bool error = false;
        bool done = false;
        double doneAt = -1.0;          // success burst / error shake
    } install;

    // Submission form
    int submitChapter = 1;
    std::vector<std::wstring> submitColors, submitFiles;
    std::wstring submitName, submitStatus;
    bool submitOk = false, sending = false, infoOpen = false;
    DiscordIdentity discord;
};

} // namespace SpeedrunLauncher::Main
