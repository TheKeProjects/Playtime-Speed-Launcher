#pragma once
#include <optional>
#include <string>

// "FullBright" (night vision) pak mod for Chapters 1 and 5. Ships the same LogicMods pak as Load
// Manip plus its own complete UE4SS build and a fullbright.playtime marker.
namespace SpeedrunLauncher::Services::FullBrightFiles {

inline constexpr const wchar_t* ConfigFileName = L"FullBright_Config.ini";

std::optional<std::wstring> ZipPath(int chapter);
std::optional<std::wstring> Ue4ssZipPath(int chapter);
std::optional<std::wstring> MarkerZipPath(int chapter);
std::optional<std::wstring> ConfigZipPath(int chapter);
// Loose copy of the bundled config (only read to show the default keys)
std::wstring LooseConfigPath();

bool IsInstalled(const std::wstring& win64Dir);
bool Install(const std::wstring& paksDir, const std::wstring& win64Dir, const std::wstring& zipPath, const std::wstring& ue4ssZipPath,
             const std::wstring& markerZipPath, const std::optional<std::wstring>& configZipPath);
void Uninstall(const std::wstring& paksDir, const std::wstring& win64Dir, const std::wstring& zipPath, const std::wstring& ue4ssZipPath,
               const std::wstring& markerZipPath, const std::optional<std::wstring>& configZipPath);

} // namespace SpeedrunLauncher::Services::FullBrightFiles
