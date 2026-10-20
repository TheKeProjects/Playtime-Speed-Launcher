#pragma once
#include <optional>
#include <string>

// "Load Manip" pak mod (Chapters 1, 4, 5): pak into Content\Paks, UE4SS build + launcher.playtime
// marker into Binaries\Win64 (Chapters 1, 4 and 5) and LoadManip_Config.ini into the project root.
namespace SpeedrunLauncher::Services::LoadManipFiles {

inline constexpr const wchar_t* ConfigFileName = L"LoadManip_Config.ini";

std::optional<std::wstring> ZipPath(int chapter);
std::optional<std::wstring> Ue4ssZipPath(int chapter);
std::optional<std::wstring> MarkerZipPath(int chapter);
std::optional<std::wstring> ConfigZipPath(int chapter);

bool IsInstalled(const std::wstring& paksDir, const std::wstring& zipPath);
// Pak into paksDir; config.ini into the project root only when it isn't there yet.
bool Install(const std::wstring& paksDir, const std::wstring& zipPath, const std::optional<std::wstring>& configZipPath);
bool InstallUe4ss(const std::wstring& win64Dir, const std::wstring& ue4ssZipPath, const std::optional<std::wstring>& markerZipPath);
void Uninstall(const std::wstring& paksDir, const std::wstring& zipPath);
void UninstallUe4ss(const std::wstring& win64Dir, const std::wstring& ue4ssZipPath, const std::optional<std::wstring>& markerZipPath);
// The UE4SS build in Win64 came from Load Manip (marker file, or its AsyncLoadToggle Lua mod).
bool IsUe4ssFromLoadManip(const std::wstring& win64Dir);
// Old Chapter 4 Load Manip (LoadManipV2_P.pak/.ucas/.utoc loose in Content\Paks): it clashes with
// the current one, so it must be deleted before installing Load Manip / FullBright.
bool HasLegacyV2(const std::wstring& paksDir);
void DeleteLegacyV2(const std::wstring& paksDir);

} // namespace SpeedrunLauncher::Services::LoadManipFiles
