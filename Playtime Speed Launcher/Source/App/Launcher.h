#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "Models/Chapter.h"
#include "Models/MultiplayerGame.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "Services/Chapters/InstallationsStore.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "Services/Fps/GameFpsService.h"
#include "Services/Hotkeys/CoresService.h"
#include "Services/Hotkeys/F11RemapService.h"
#include "Services/Platforms/EpicGamesService.h"

namespace SpeedrunLauncher::App {

// Which mods a chapter's active install has (cached: computing it walks the game folders).
struct ChapterModState {
    std::optional<std::wstring> win64Dir;
    bool ue4ss = false;        // standalone UE4SS (not one owned by Load Manip / FullBright)
    bool loadManip = false;
    bool fullBright = false;
};

// Everything the launcher shares between its windows, screens and features: the chapters, the
// stores, the long-lived services and the game-watcher state.
class Launcher {
public:
    static Launcher& Get();
    void Initialize();
    void Shutdown();

    std::vector<Models::Chapter> chapters;
    Services::InstallationsStore store;
    Services::ChapterPlaytimeStore playtime;
    Services::EpicGamesService epic;
    std::unique_ptr<Services::DiscordPresenceService> discord;
    Services::GameFpsService fps;
    Services::CoresService cores;
    Services::F11RemapService f11;

    int selected = 0;
    // Main screen tab: the chapters or the multiplayer games (with their own selection)
    enum class Section { Chapters, Multiplayers };
    Section section = Section::Chapters;
    std::vector<Models::MultiplayerGame> multiplayers;
    int selectedMultiplayer = 0;
    std::vector<bool> multiplayerRunning;   // per multiplayer index (game watcher)
    std::vector<uint32_t> multiplayerPid;
    bool InMultiplayers() const { return section == Section::Multiplayers; }
    Models::MultiplayerGame& SelectedMultiplayer() { return multiplayers[static_cast<size_t>(selectedMultiplayer)]; }
    bool AnyMultiplayerRunning() const;

    std::vector<bool> gameRunning;       // per chapter index (refreshed every 2 s by the game watcher)
    std::vector<uint32_t> runningPid;

    Models::Chapter& Selected() { return chapters[static_cast<size_t>(selected)]; }
    Models::Chapter* ChapterByNumber(int number);
    int IndexOf(int number) const;

    // The exe to launch: manual selection → Epic (when enabled) → Steam auto-detect.
    std::optional<std::wstring> ActiveExePath(const Models::Chapter& chapter) const;
    std::wstring VersionLabel(const Models::Chapter& chapter) const;
    const Models::Chapter* RunningChapter() const;
    bool AnyGameRunning() const;
    // Thread-safe (also called from the input hook thread): uses the snapshot published by the
    // game watcher, never the vectors above.
    bool IsGameForeground() const;
    int ForegroundChapter() const;   // number of the running chapter (or multiplayer playtimeId) whose window has focus, 0 if none
    void PublishRunningGames();      // game watcher: after updating gameRunning / runningPid
    // "*-Win64-Shipping" binary name for an install (cached per exe path)
    std::optional<std::wstring> ShippingExeName(const std::wstring& exePath);

    static std::optional<std::wstring> FindWin64Dir(const std::wstring& startDir);
    static bool IsUe4ssInstalled(const std::wstring& win64Dir);
    static bool HasLoadManipMarker(const std::wstring& win64Dir);
    static bool IsLoadManipInstalled(const std::wstring& paksDir, const std::optional<std::wstring>& zipPath,
                                     const std::optional<std::wstring>& win64Dir, int chapter);
    static bool IsUe4ssOwnedByMod(const std::wstring& win64Dir, int chapter);
    bool IsUe4ssActiveForChapter(const Models::Chapter& chapter) const;

    const ChapterModState& ModState(size_t index) const;
    void RefreshModStates();

    // LoadManip_Config.ini / FullBright_Config.ini of the active install, if installed.
    std::optional<std::wstring> ActiveLoadManipConfigPath(int chapter) const;
    std::optional<std::wstring> ActiveFullBrightConfigPath(int chapter) const;

private:
    std::map<std::wstring, std::optional<std::wstring>> shippingNames;
    std::vector<ChapterModState> modStates;
    mutable std::mutex runningMutex;
    std::vector<std::pair<uint32_t, int>> runningSnapshot;   // (pid, chapter number) of running games
};

inline Launcher& L() { return Launcher::Get(); }

} // namespace SpeedrunLauncher::App
