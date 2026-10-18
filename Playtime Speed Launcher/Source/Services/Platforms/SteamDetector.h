#pragma once
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Models/Chapter.h"
#include "Models/MultiplayerGame.h"

namespace SpeedrunLauncher::Services::SteamDetector {

// Finds each chapter's Steam install (fills gameExePath / detectedVersion). Slow: run on a worker.
void DetectAll(std::vector<Models::Chapter>& chapters);
// Same for the multiplayer games (their exe at a fixed path inside each Steam library). Slow too.
void DetectMultiplayers(std::vector<Models::MultiplayerGame>& games);

// Folder produced by SteamCMD's download_depot (steamapps\content\app_X\depot_Y), or nullopt.
std::optional<std::wstring> FindDepotDownloadPath(int appId, int depotId, const std::optional<std::wstring>& steamCmdPath);

// Main game exe inside a folder (biggest exe that isn't a crash reporter / installer / launcher).
std::optional<std::wstring> FindGameExe(const std::wstring& gamePath);

std::wstring ExecutableDirectory();
std::vector<std::wstring> FixedDrives();   // "C:\" ...
std::wstring FileProductVersion(const std::wstring& exe);

} // namespace SpeedrunLauncher::Services::SteamDetector
