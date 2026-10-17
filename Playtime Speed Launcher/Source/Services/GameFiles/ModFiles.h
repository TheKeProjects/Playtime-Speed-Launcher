#pragma once
#include <optional>
#include <string>

// Helpers shared by the pak / UE4SS mod installers.
namespace SpeedrunLauncher::Services::ModFiles {

// .../Binaries/Win64 → project root (folder containing Binaries/Content)
std::optional<std::wstring> ProjectRoot(const std::wstring& win64Dir);
// .../Binaries/Win64 → <project root>\Content\Paks
std::optional<std::wstring> PaksDir(const std::wstring& win64Dir);

// Removes exactly the files (and folders) a zip ships from `targetDir`, clearing read-only flags.
void DeleteZipContents(const std::wstring& targetDir, const std::wstring& zipPath);
// True when any file the zip ships exists under `targetDir`.
bool AnyZipFileExists(const std::wstring& targetDir, const std::wstring& zipPath);

// Rewrites one "Key = Value" line of an ini, keeping every other line as-is.
void UpdateConfigKey(const std::wstring& configPath, const std::wstring& key, const std::wstring& value);
// Reads "Key = Value" pairs (case-insensitive key).
std::optional<std::wstring> ReadConfigKey(const std::wstring& configPath, const std::wstring& key);

std::wstring ToolPath(const std::wstring& relative);   // TempDir\Assets\Tools\<relative>

} // namespace SpeedrunLauncher::Services::ModFiles
