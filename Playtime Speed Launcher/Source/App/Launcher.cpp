#include "App/Launcher.h"
#include <filesystem>
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/GameFiles/FullBrightFiles.h"
#include "Services/GameFiles/LoadManipFiles.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::App {

namespace {
const ChapterModState kNoMods;
}

Launcher& Launcher::Get() {
    static Launcher launcher;
    return launcher;
}

void Launcher::Initialize() {
    chapters = Models::Chapter::All();
    multiplayers = Models::MultiplayerGame::All();
    multiplayerRunning.assign(multiplayers.size(), false);
    store = Services::InstallationsStore::Load();
    gameRunning.assign(chapters.size(), false);
    runningPid.assign(chapters.size(), 0);
    modStates.assign(chapters.size(), ChapterModState{});
    discord = std::make_unique<Services::DiscordPresenceService>();
}

void Launcher::Shutdown() {
    cores.RestoreIfActive();
    fps.Stop();
    discord.reset();
}

Models::Chapter* Launcher::ChapterByNumber(int number) {
    for (auto& c : chapters)
        if (c.number == number) return &c;
    return nullptr;
}

int Launcher::IndexOf(int number) const {
    for (size_t i = 0; i < chapters.size(); ++i)
        if (chapters[i].number == number) return static_cast<int>(i);
    return -1;
}

std::optional<std::wstring> Launcher::ActiveExePath(const Models::Chapter& chapter) const {
    if (auto selectedPath = store.SelectedPath(chapter.number)) return selectedPath;
    // In Epic mode never fall back to the Steam exe: no Epic path = not installed
    if (epic.IsEnabled()) return epic.ExePath(chapter.number);
    return chapter.gameExePath;
}

std::wstring Launcher::VersionLabel(const Models::Chapter& chapter) const {
    if (auto selectedPath = store.SelectedPath(chapter.number)) {
        for (const auto& custom : store.Customs(chapter.number))
            if (custom.exePath == *selectedPath) return custom.name;
        return Files::FileNameWithoutExtension(*selectedPath);
    }
    if (epic.IsEnabled() && epic.ExePath(chapter.number)) return Loc::Get("version_auto_epic");
    if (chapter.IsInstalled()) return Loc::Get("version_auto_steam");
    return chapter.isAvailable ? Loc::Get("version_not_installed") : Loc::Get("version_none");
}

const Models::Chapter* Launcher::RunningChapter() const {
    for (size_t i = 0; i < chapters.size(); ++i)
        if (gameRunning[i]) return &chapters[i];
    return nullptr;
}

bool Launcher::AnyGameRunning() const {
    for (bool running : gameRunning)
        if (running) return true;
    return false;
}

bool Launcher::AnyMultiplayerRunning() const {
    for (bool running : multiplayerRunning)
        if (running) return true;
    return false;
}

bool Launcher::IsGameForeground() const { return ForegroundChapter() != 0; }

int Launcher::ForegroundChapter() const {
    // Uses the PIDs cached by the game watcher: this runs on every key press while a hook is active
    const uint32_t pid = Process::ForegroundPid();
    if (!pid) return 0;
    std::lock_guard lock(runningMutex);
    for (const auto& [runningPidValue, number] : runningSnapshot)
        if (runningPidValue == pid) return number;
    return 0;
}

void Launcher::PublishRunningGames() {
    std::vector<std::pair<uint32_t, int>> snapshot;
    for (size_t i = 0; i < chapters.size(); ++i)
        if (gameRunning[i] && runningPid[i]) snapshot.emplace_back(runningPid[i], chapters[i].number);
    std::lock_guard lock(runningMutex);
    runningSnapshot = std::move(snapshot);
}

std::optional<std::wstring> Launcher::ShippingExeName(const std::wstring& exePath) {
    auto it = shippingNames.find(exePath);
    if (it != shippingNames.end()) return it->second;
    std::optional<std::wstring> name;
    if (auto win64 = FindWin64Dir(Files::DirectoryName(exePath))) {
        auto shipping = Files::ListFiles(*win64, L"*-Win64-Shipping.exe");
        if (!shipping.empty()) name = Files::FileNameWithoutExtension(shipping.front());
    }
    shippingNames[exePath] = name;
    return name;
}

// The game's own Binaries\Win64 (the one with the *-Win64-Shipping.exe). Searched level by level,
// and a Win64 folder without the shipping exe only counts when there is no better one: Unreal
// installs also have Engine\Binaries\ThirdParty\...\Win64 folders (PhysX, Ogg, NVIDIA...), and
// a depth-first walk reached those first for Chapters 2 and 3.
std::optional<std::wstring> Launcher::FindWin64Dir(const std::wstring& startDir) {
    if (startDir.empty()) return std::nullopt;
    if (Text::Equals(Files::FileName(startDir), L"Win64", true)) return startDir;
    namespace fs = std::filesystem;
    std::optional<std::wstring> fallback;
    std::vector<fs::path> level{ fs::path(startDir) };
    for (int depth = 0; depth < 8 && !level.empty(); ++depth) {
        std::vector<fs::path> next;
        for (const auto& dir : level) {
            std::error_code ec;
            for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                 !ec && it != fs::directory_iterator(); it.increment(ec)) {
                if (!it->is_directory(ec)) continue;
                const fs::path& path = it->path();
                if (Text::Equals(path.filename().wstring(), L"Win64", true)) {
                    if (!Files::ListFiles(path.wstring(), L"*-Win64-Shipping.exe").empty()) return path.wstring();
                    if (!fallback) fallback = path.wstring();
                } else {
                    next.push_back(path);
                }
            }
        }
        level = std::move(next);
    }
    return fallback;
}

bool Launcher::IsUe4ssInstalled(const std::wstring& win64Dir) {
    return Files::Exists(Files::Combine(win64Dir, L"dwmapi.dll")) || Files::DirectoryExists(Files::Combine(win64Dir, L"ue4ss"));
}

// Only Load Manip's own marker (FullBright's standalone UE4SS ships the AsyncLoadToggle folder too)
bool Launcher::HasLoadManipMarker(const std::wstring& win64Dir) { return Files::Exists(Files::Combine(win64Dir, L"launcher.playtime")); }

bool Launcher::IsLoadManipInstalled(const std::wstring& paksDir, const std::optional<std::wstring>& zipPath,
                                    const std::optional<std::wstring>& win64Dir, int chapter) {
    if (!zipPath || !Services::LoadManipFiles::IsInstalled(paksDir, *zipPath)) return false;
    // FullBright lays down the same pak on chapters 1, 4 and 5: Load Manip's marker must be there too
    if ((chapter == 1 || chapter == 4 || chapter == 5) && win64Dir) return HasLoadManipMarker(*win64Dir);
    return true;
}

bool Launcher::IsUe4ssOwnedByMod(const std::wstring& win64Dir, int chapter) {
    return Services::LoadManipFiles::IsUe4ssFromLoadManip(win64Dir) ||
           ((chapter == 1 || chapter == 4 || chapter == 5) && Services::FullBrightFiles::IsInstalled(win64Dir));
}

bool Launcher::IsUe4ssActiveForChapter(const Models::Chapter& chapter) const {
    auto exe = ActiveExePath(chapter);
    if (!exe || exe->empty()) return false;
    auto win64 = FindWin64Dir(Files::DirectoryName(*exe));
    return win64 && IsUe4ssInstalled(*win64) && !IsUe4ssOwnedByMod(*win64, chapter.number);
}

const ChapterModState& Launcher::ModState(size_t index) const { return index < modStates.size() ? modStates[index] : kNoMods; }

void Launcher::RefreshModStates() {
    modStates.assign(chapters.size(), ChapterModState{});
    for (size_t i = 0; i < chapters.size(); ++i) {
        const auto& chapter = chapters[i];
        auto exe = ActiveExePath(chapter);
        if (!exe || exe->empty()) continue;
        auto win64 = FindWin64Dir(Files::DirectoryName(*exe));
        if (!win64) continue;
        auto& state = modStates[i];
        state.win64Dir = win64;
        state.ue4ss = IsUe4ssInstalled(*win64) && !IsUe4ssOwnedByMod(*win64, chapter.number);
        if (auto paks = Services::ModFiles::PaksDir(*win64))
            state.loadManip = IsLoadManipInstalled(*paks, Services::LoadManipFiles::ZipPath(chapter.number), win64, chapter.number);
        state.fullBright = Services::FullBrightFiles::IsInstalled(*win64);
    }
}

std::optional<std::wstring> Launcher::ActiveLoadManipConfigPath(int chapterNumber) const {
    const int index = IndexOf(chapterNumber);
    if (index < 0) return std::nullopt;
    auto exe = ActiveExePath(chapters[static_cast<size_t>(index)]);
    if (!exe || exe->empty()) return std::nullopt;
    auto win64 = FindWin64Dir(Files::DirectoryName(*exe));
    if (!win64) return std::nullopt;
    auto root = Services::ModFiles::ProjectRoot(*win64);
    if (!root) return std::nullopt;
    const std::wstring path = Files::Combine(*root, Services::LoadManipFiles::ConfigFileName);
    if (!Files::Exists(path)) return std::nullopt;
    return path;
}

std::optional<std::wstring> Launcher::ActiveFullBrightConfigPath(int chapterNumber) const {
    const int index = IndexOf(chapterNumber);
    if (index < 0) return std::nullopt;
    auto exe = ActiveExePath(chapters[static_cast<size_t>(index)]);
    if (!exe || exe->empty()) return std::nullopt;
    auto win64 = FindWin64Dir(Files::DirectoryName(*exe));
    if (!win64) return std::nullopt;
    auto root = Services::ModFiles::ProjectRoot(*win64);
    if (!root) return std::nullopt;
    const std::wstring path = Files::Combine(*root, Services::FullBrightFiles::ConfigFileName);
    if (!Files::Exists(path)) return std::nullopt;
    return path;
}

} // namespace SpeedrunLauncher::App
