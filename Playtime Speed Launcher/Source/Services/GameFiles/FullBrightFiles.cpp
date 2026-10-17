#include "Services/GameFiles/FullBrightFiles.h"
#include "Core/Files.h"
#include "Core/Zip.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::Services::FullBrightFiles {

namespace {

std::optional<std::wstring> ForChapter(int chapter, const wchar_t* ch1, const wchar_t* ch5) {
    if (chapter == 1) return ModFiles::ToolPath(std::wstring(L"Chapter 1 Night Vision\\FullBright\\") + ch1);
    if (chapter == 5) return ModFiles::ToolPath(std::wstring(L"Chapter 5 Night Vision\\FullBright\\") + ch5);
    return std::nullopt;
}

} // namespace

std::optional<std::wstring> ZipPath(int chapter) { return ForChapter(chapter, L"FullBrightCH1.zip", L"FullBrightCH5.zip"); }
std::optional<std::wstring> Ue4ssZipPath(int chapter) { return ForChapter(chapter, L"FullBrightCH1UE4SS.zip", L"FullBrightCH5UE4SS.zip"); }
std::optional<std::wstring> MarkerZipPath(int chapter) {
    return ForChapter(chapter, L"FullBrightCH1Playtime.zip", L"FullBrightCH5Playtime.zip");
}
std::optional<std::wstring> ConfigZipPath(int chapter) { return ForChapter(chapter, L"ConfigFullBrightCH1.zip", L"ConfigFullBrightCH5.zip"); }

std::wstring LooseConfigPath() { return ModFiles::ToolPath(std::wstring(L"Chapter 1 Night Vision\\FullBright\\") + ConfigFileName); }

bool IsInstalled(const std::wstring& win64Dir) { return Files::Exists(Files::Combine(win64Dir, L"fullbright.playtime")); }

bool Install(const std::wstring& paksDir, const std::wstring& win64Dir, const std::wstring& zipPath, const std::wstring& ue4ssZipPath,
             const std::wstring& markerZipPath, const std::optional<std::wstring>& configZipPath) {
    Files::CreateDirectories(paksDir);
    if (!Zip::ExtractToDirectory(zipPath, paksDir, true)) return false;
    Files::CreateDirectories(win64Dir);
    if (!Zip::ExtractToDirectory(ue4ssZipPath, win64Dir, true)) return false;
    if (!Zip::ExtractToDirectory(markerZipPath, win64Dir, true)) return false;
    if (configZipPath && Files::Exists(*configZipPath)) {
        auto root = ModFiles::ProjectRoot(win64Dir);
        if (root && !Files::Exists(Files::Combine(*root, ConfigFileName))) Zip::ExtractToDirectory(*configZipPath, *root, false);
    }
    return true;
}

void Uninstall(const std::wstring& paksDir, const std::wstring& win64Dir, const std::wstring& zipPath, const std::wstring& ue4ssZipPath,
               const std::wstring& markerZipPath, const std::optional<std::wstring>& configZipPath) {
    ModFiles::DeleteZipContents(paksDir, zipPath);
    ModFiles::DeleteZipContents(win64Dir, ue4ssZipPath);
    ModFiles::DeleteZipContents(win64Dir, markerZipPath);
    if (configZipPath && Files::Exists(*configZipPath)) {
        if (auto root = ModFiles::ProjectRoot(win64Dir)) ModFiles::DeleteZipContents(*root, *configZipPath);
    }
}

} // namespace SpeedrunLauncher::Services::FullBrightFiles
