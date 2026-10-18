#pragma once
#include <memory>
#include <string>
#include <vector>
#include "App/Launcher.h"
#include "UI/Window.h"
#include "Windows/Main/MainScreen/ChapterCards.h"
#include "Windows/Main/MainScreen/ChapterInfoPanel.h"
#include "Windows/Main/MainScreen/IntroVideo.h"
#include "Windows/Main/MainScreen/SteamUserBadge.h"
#include "Windows/Toasts/ToastWindow.h"

namespace SpeedrunLauncher::Features {
class GameWatcher;
class GlobalHotkeys;
class InputRemaps;
class EasterEggs;
class LiveSplitPoller;
class GameOverlays;
} // namespace SpeedrunLauncher::Features

namespace SpeedrunLauncher::Main {

class Overlay;
class SettingsOverlay;
class VersionsOverlay;
class UpdatesOverlay;
class LiveSplitOverlay;
class SaveCardOverlay;
class CheckpointOverlay;
class BugReportOverlay;
class ChangelogOverlay;
class EasterEggOverlay;
class ModPopups;
class AddToSteamOverlay;
class HandModsOverlay;
class TutorialPopups;
class KeyClickWizard;

// The launcher's main window: the chapter carousel screen plus every in-window overlay.
// Screen pieces live in MainScreen\, overlays in Overlays\ and Settings\, the background logic
// (game watcher, hotkeys, remaps, easter eggs...) in Features\.
class MainWindow : public UI::Window {
public:
    MainWindow();
    ~MainWindow() override;
    bool Open();
    static MainWindow* Instance();

    // ── Actions used by the screen and the overlays ──
    void SelectChapter(int index);
    void SelectMultiplayer(int index);
    void SelectSection(App::Launcher::Section section);   // Chapters / Multiplayers tabs
    void RefreshInfo();                 // status / version / mod buttons after anything changed
    void ApplyLanguage();
    void ApplySeasonalPalette();        // after the icon theme changes
    void ApplyIconTheme();
    void PlaySelected();
    void TogglePlatform();
    void Quit();
    void Minimize() { ShowWindow(hwnd, SW_MINIMIZE); }
    void ApplyWindowMode();

    void OpenSettings();
    void OpenVersions();
    void OpenHandMods();
    void OpenLeaderboard();
    void OpenBugReport();
    void OpenChangelog();
    void OpenBeginnerTutorials();
    void OpenUpdates();
    void OpenLiveSplit();
    void OpenSaveCard(int chapter);
    void OpenUe4ss(int chapter);
    void OpenLoadManip(int chapter);
    void OpenFullBright(int chapter);
    void OpenAddToSteam();
    void OpenKeyClickWizard(int bindingIndex = -1);   // Settings → Controls → Key to click
    // (Re-)registers the key-to-click on/off shortcut; false when another app already owns it
    bool ApplyKeyClickHotkey();
    bool KeyClickHotkeyOk() const { return keyClickHotkeyOk; }
    void ToggleKeyClick();   // the shortcut: on ↔ off, with a toast

    void ShowError(const std::wstring& message);
    void ShowGameToast();
    void ShowTutorialToast();
    Toasts::ToastSlot& CoresToast() { return coresToast; }
    Toasts::ToastSlot& Ue4ssToast() { return ue4ssToast; }

    // ── State shown by the screen ──
    const std::wstring& OnlineText() const { return onlineText; }
    bool UpdateAlert() const { return updateAlert; }
    // A game is running or another launcher window is on top: ambient animations rest
    bool Resting() const { return resting; }
    void SetUpdateAlert(bool on) { updateAlert = on; }
    bool LiveSplitUpdateAlert() const { return liveSplitUpdateAlert; }
    void SetLiveSplitUpdateAlert(bool on) { liveSplitUpdateAlert = on; }
    bool IntroPlaying() const { return intro.Visible(); }
    void HideIntro();

    ChapterInfoPanel& Info() { return info; }
    Features::GameWatcher& Watcher() { return *watcher; }
    Features::GlobalHotkeys& Hotkeys() { return *hotkeys; }
    Features::InputRemaps& Remaps() { return *remaps; }
    Features::EasterEggs& Eggs() { return *eggs; }
    Features::GameOverlays& GameOverlays() { return *gameOverlays; }
    SettingsOverlay& Settings() { return *settings; }
    UpdatesOverlay& Updates() { return *updates; }
    LiveSplitOverlay& LiveSplit() { return *liveSplit; }
    VersionsOverlay& Versions() { return *versions; }
    ModPopups& Mods() { return *mods; }
    EasterEggOverlay& EggVideo() { return *eggVideo; }
    TutorialPopups& Tutorials() { return *tutorials; }

protected:
    void OnFrame() override;
    bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) override;
    void OnClosing(bool& cancel) override;
    void OnDestroyed() override;
    Color ClearColor() const override;

private:
    void DrawMainScreen(const Rect& screen);
    void StartBackgroundWork();

    ChapterCards cards{ *this };
    ChapterInfoPanel info{ *this };
    SteamUserBadge steamUser;
    IntroVideo intro;
    Toasts::ToastSlot gameToast, tutorialToast, coresToast, ue4ssToast;

    std::unique_ptr<Features::GameWatcher> watcher;
    std::unique_ptr<Features::GlobalHotkeys> hotkeys;
    std::unique_ptr<Features::InputRemaps> remaps;
    std::unique_ptr<Features::EasterEggs> eggs;
    std::unique_ptr<Features::LiveSplitPoller> liveSplitPoller;
    std::unique_ptr<Features::GameOverlays> gameOverlays;

    std::unique_ptr<SettingsOverlay> settings;
    std::unique_ptr<VersionsOverlay> versions;
    std::unique_ptr<UpdatesOverlay> updates;
    std::unique_ptr<LiveSplitOverlay> liveSplit;
    std::unique_ptr<SaveCardOverlay> saveCard;
    std::unique_ptr<CheckpointOverlay> checkpoint;
    std::unique_ptr<BugReportOverlay> bugReport;
    std::unique_ptr<ChangelogOverlay> changelog;
    std::unique_ptr<EasterEggOverlay> eggVideo;
    std::unique_ptr<ModPopups> mods;
    std::unique_ptr<AddToSteamOverlay> addToSteam;
    std::unique_ptr<HandModsOverlay> handMods;
    std::unique_ptr<TutorialPopups> tutorials;
    std::unique_ptr<KeyClickWizard> keyClickWizard;
    bool keyClickHotkeyOk = true;
    std::vector<unsigned> recentKeys;   // last key presses in the launcher (secret ↑↑↑↑↑↓↓↓↓↓)
    void CheckSpamCombo(unsigned vk);
    static constexpr int kKeyClickHotkeyId = 9003;
    std::vector<Overlay*> overlays;   // z-order (first = bottom)

    std::wstring onlineText;
    bool updateAlert = false;
    bool liveSplitUpdateAlert = false;
    unsigned seenSeason = 0;
    double seasonChangedAt = -10.0;   // the new scenery fades in after a theme change
    bool resting = false;
    // While a menu is open the main screen (dimmed behind it) is static: it's drawn once into
    // this snapshot and the snapshot is reused, so each frame only costs the menu itself
    bool menuOpen = false;
    ComPtr<ID2D1Bitmap1> screenSnapshot;
    bool snapshotDirty = true;
    double snapshotAt = -10.0;
};

} // namespace SpeedrunLauncher::Main
