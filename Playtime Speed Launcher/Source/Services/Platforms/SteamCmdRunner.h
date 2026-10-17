#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include "Core/Async.h"

namespace SpeedrunLauncher::Services::SteamCmd {

struct SteamUser {
    std::wstring personaName;
    std::wstring steamId64;
};

std::optional<std::wstring> Find();                       // steamcmd.exe, if present
std::optional<std::wstring> SteamInstallPath();           // from the registry
std::optional<std::wstring> LoggedInUsername();           // AccountName of the most recent login
std::optional<SteamUser> ActiveUser();                    // most recent loginusers.vdf entry
std::optional<std::wstring> AvatarPath(const std::wstring& steamId64);
void CopyCredentials(const std::wstring& steamCmdPath);   // Steam's config.vdf → SteamCMD's config folder

enum class DownloadStage { Downloading, Extracting };
// `progress(stage, received, total)` (total is 0 while unknown / extracting), from the worker thread.
using DownloadProgress = std::function<void(DownloadStage stage, uint64_t received, uint64_t total)>;

// Downloads steamcmd.zip into %LOCALAPPDATA%\SpeedrunLauncher\steamcmd and extracts it.
// Blocking. Returns the exe path, or nullopt with `error` set (empty when `cancel` stopped it).
std::optional<std::wstring> Download(const DownloadProgress& progress, std::wstring* error, const Async::Cancel& cancel = {});

// Runs "+login user [pass] +download_depot app depot manifest +quit" hidden. Blocking (worker thread).
// `line` gets every output line; `prompt` is called (on this worker thread) when SteamCMD asks for a
// Steam Guard / two-factor code — return the code, or nullopt to skip.
void Run(const std::wstring& steamCmdPath, const std::wstring& username, const std::optional<std::wstring>& password, int appId,
         int depotId, const std::wstring& manifestId, const std::function<void(const std::wstring&)>& line,
         const std::function<std::optional<std::wstring>(const std::wstring&)>& prompt, const Async::Cancel& cancel);

} // namespace SpeedrunLauncher::Services::SteamCmd
