#pragma once
#include <optional>
#include <string>
#include <vector>

// Escape From Playtime "Custom Name" mod (old versions): a UE4SS Lua mod that renames you in game
// (and styles / animates your name on your screen). Turned on and off per version, like Load Manip:
//   Win64\ue4ss\Mods\PlaytimeCustomName\{Scripts\main.lua, enabled.txt, config.ini}
// When the version has no UE4SS yet, the Chapter 5 build (same engine, UE 5.6) is added with a
// customname.playtime marker, so removing the mod also removes the UE4SS it brought.
// Install / Uninstall / SyncAll block on file work: run them off the UI thread.
namespace SpeedrunLauncher::Services::CustomNameFiles {

inline constexpr const wchar_t* ModFolder = L"PlaytimeCustomName";
inline constexpr const wchar_t* Marker = L"customname.playtime";

struct Result {
    bool ok = false;
    std::wstring message;
};

std::optional<std::wstring> Win64ForExe(const std::wstring& exePath);
bool IsInstalled(const std::wstring& win64Dir);   // cheap probe
Result Install(const std::wstring& win64Dir, const std::wstring& config);
Result Uninstall(const std::wstring& win64Dir);
// Rewrites config.ini in one install (the mod re-reads it every 2 s, even with the game open)
bool WriteConfig(const std::wstring& win64Dir, const std::wstring& config);
// Writes `config` into every one of `exePaths` (the Escape From Playtime old versions) that has the mod
void SyncAll(const std::vector<std::wstring>& exePaths, const std::wstring& config);

} // namespace SpeedrunLauncher::Services::CustomNameFiles
