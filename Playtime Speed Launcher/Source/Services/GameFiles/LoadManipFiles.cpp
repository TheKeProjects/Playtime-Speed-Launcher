#include "Services/GameFiles/LoadManipFiles.h"
#include "Core/Files.h"
#include "Core/Zip.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::Services::LoadManipFiles {

std::optional<std::wstring> ZipPath(int chapter) {
    switch (chapter) {
        case 1: return ModFiles::ToolPath(L"Load Manip Chapter 1\\LoadManipCH1.zip");
        case 4: return ModFiles::ToolPath(L"Load Manip Chapter 4\\LoadManipCH4.zip");
        case 5: return ModFiles::ToolPath(L"Load Manip Chapter 5\\LoadManipCH5.zip");
        default: return std::nullopt;
    }
}

std::optional<std::wstring> Ue4ssZipPath(int chapter) {
    switch (chapter) {
        case 1: return ModFiles::ToolPath(L"Load Manip Chapter 1\\LoadManipCH1UE4SS.zip");
        case 4: return ModFiles::ToolPath(L"Load Manip Chapter 4\\LoadManipCH4UE4SS.zip");
        case 5: return ModFiles::ToolPath(L"Load Manip Chapter 5\\LoadManipCH5UE4SS.zip");
        default: return std::nullopt;
    }
}

std::optional<std::wstring> MarkerZipPath(int chapter) {
    switch (chapter) {
        case 1: return ModFiles::ToolPath(L"Load Manip Chapter 1\\LoadManipCH1Playtime.zip");
        case 4: return ModFiles::ToolPath(L"Load Manip Chapter 4\\LoadManipCH4Playtime.zip");
        case 5: return ModFiles::ToolPath(L"Load Manip Chapter 5\\LoadManipCH5Playtime.zip");
        default: return std::nullopt;
    }
}

std::optional<std::wstring> ConfigZipPath(int chapter) {
    switch (chapter) {
        case 1: return ModFiles::ToolPath(L"Load Manip Chapter 1\\ConfigLoadManipCH1.zip");
        case 4: return ModFiles::ToolPath(L"Load Manip Chapter 4\\ConfigLoadManipCH4.zip");
        case 5: return ModFiles::ToolPath(L"Load Manip Chapter 5\\ConfigLoadManipCH5.zip");
        default: return std::nullopt;
    }
}

bool IsInstalled(const std::wstring& paksDir, const std::wstring& zipPath) {
    if (!Files::Exists(zipPath)) return false;
    return ModFiles::AnyZipFileExists(paksDir, zipPath);
}

bool Install(const std::wstring& paksDir, const std::wstring& zipPath, const std::optional<std::wstring>& configZipPath) {
    Files::CreateDirectories(paksDir);
    if (!Zip::ExtractToDirectory(zipPath, paksDir, true)) return false;
    if (configZipPath && Files::Exists(*configZipPath)) {
        const std::wstring content = Files::DirectoryName(paksDir);
        const std::wstring root = Files::DirectoryName(content);
        if (!root.empty() && !Files::Exists(Files::Combine(root, ConfigFileName))) Zip::ExtractToDirectory(*configZipPath, root, false);
    }
    return true;
}

bool InstallUe4ss(const std::wstring& win64Dir, const std::wstring& ue4ssZipPath, const std::optional<std::wstring>& markerZipPath) {
    Files::CreateDirectories(win64Dir);
    if (!Zip::ExtractToDirectory(ue4ssZipPath, win64Dir, true)) return false;
    if (markerZipPath && Files::Exists(*markerZipPath)) Zip::ExtractToDirectory(*markerZipPath, win64Dir, true);
    return true;
}

void Uninstall(const std::wstring& paksDir, const std::wstring& zipPath) { ModFiles::DeleteZipContents(paksDir, zipPath); }

void UninstallUe4ss(const std::wstring& win64Dir, const std::wstring& ue4ssZipPath, const std::optional<std::wstring>& markerZipPath) {
    ModFiles::DeleteZipContents(win64Dir, ue4ssZipPath);
    if (markerZipPath && Files::Exists(*markerZipPath)) ModFiles::DeleteZipContents(win64Dir, *markerZipPath);
}

bool IsUe4ssFromLoadManip(const std::wstring& win64Dir) {
    return Files::Exists(Files::Combine(win64Dir, L"launcher.playtime")) ||
           Files::DirectoryExists(Files::Combine(win64Dir, L"ue4ss", L"Mods", L"AsyncLoadToggle"));
}

namespace {
constexpr const wchar_t* kLegacyV2Files[] = { L"LoadManipV2_P.pak", L"LoadManipV2_P.ucas", L"LoadManipV2_P.utoc" };
}

bool HasLegacyV2(const std::wstring& paksDir) {
    for (const wchar_t* name : kLegacyV2Files)
        if (Files::Exists(Files::Combine(paksDir, name))) return true;
    return false;
}

void DeleteLegacyV2(const std::wstring& paksDir) {
    for (const wchar_t* name : kLegacyV2Files) Files::Delete(Files::Combine(paksDir, name));
}

} // namespace SpeedrunLauncher::Services::LoadManipFiles
