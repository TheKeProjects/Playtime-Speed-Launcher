#include "Services/GameFiles/FullBrightFiles.h"
#include "Core/Files.h"
#include "Core/Zip.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::Services::FullBrightFiles {

namespace {

// "Chapter N Night Vision\FullBright\<prefix>CHN<suffix>"
std::optional<std::wstring> ForChapter(int chapter, const wchar_t* prefix, const wchar_t* suffix) {
    if (chapter != 1 && chapter != 4 && chapter != 5) return std::nullopt;
    const std::wstring n = std::to_wstring(chapter);
    return ModFiles::ToolPath(L"Chapter " + n + L" Night Vision\\FullBright\\" + prefix + L"CH" + n + suffix);
}

} // namespace

std::optional<std::wstring> ZipPath(int chapter) { return ForChapter(chapter, L"FullBright", L".zip"); }
std::optional<std::wstring> Ue4ssZipPath(int chapter) { return ForChapter(chapter, L"FullBright", L"UE4SS.zip"); }
std::optional<std::wstring> MarkerZipPath(int chapter) { return ForChapter(chapter, L"FullBright", L"Playtime.zip"); }
std::optional<std::wstring> ConfigZipPath(int chapter) { return ForChapter(chapter, L"ConfigFullBright", L".zip"); }

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
