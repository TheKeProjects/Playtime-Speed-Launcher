#include "Services/GameFiles/ModInstaller.h"
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Zip.h"
#include "Services/GameFiles/FullBrightFiles.h"
#include "Services/GameFiles/LoadManipFiles.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::Services::ModInstaller {

namespace {

bool HasFile(const std::optional<std::wstring>& path) { return path && Files::Exists(*path); }
bool UsesUe4ssMods(int chapter) { return chapter == 1 || chapter == 5; }

Result Ok(std::wstring message) { return { true, std::move(message) }; }
Result Fail(std::wstring message) { return { false, std::move(message) }; }

const wchar_t* kNoGame = L"No game path found for this chapter.\nPlease set up the game installation first.";

bool Extract(const std::wstring& zip, const std::wstring& dir, std::wstring& error) {
    Files::CreateDirectories(dir);
    return Zip::ExtractToDirectory(zip, dir, true, &error);
}

struct FullBrightZips {
    std::optional<std::wstring> pak, ue4ss, marker, config;
    bool Complete() const { return HasFile(pak) && HasFile(ue4ss) && HasFile(marker); }
};

FullBrightZips FullBrightZipsFor(int chapter) {
    return { FullBrightFiles::ZipPath(chapter), FullBrightFiles::Ue4ssZipPath(chapter), FullBrightFiles::MarkerZipPath(chapter),
             FullBrightFiles::ConfigZipPath(chapter) };
}

// Keeps config.ini (the user's keybinds): only pak / UE4SS / marker go
void UninstallFullBrightKeepConfig(const Target& t, const FullBrightZips& z) {
    FullBrightFiles::Uninstall(*t.paks, *t.win64, *z.pak, *z.ue4ss, *z.marker, std::nullopt);
}

} // namespace

Target Resolve(int chapter) {
    Target t;
    t.chapter = chapter;
    auto& l = App::L();
    if (auto* ch = l.ChapterByNumber(chapter)) {
        if (auto exe = l.ActiveExePath(*ch); exe && !exe->empty()) t.win64 = App::Launcher::FindWin64Dir(Files::DirectoryName(*exe));
    }
    if (t.win64) t.paks = ModFiles::PaksDir(*t.win64);
    return t;
}

bool Ue4ssPresent(const Target& t) { return t.win64 && App::Launcher::IsUe4ssInstalled(*t.win64); }
bool LoadManipMarker(const Target& t) { return t.win64 && App::Launcher::HasLoadManipMarker(*t.win64); }
bool FullBrightInstalled(const Target& t) { return UsesUe4ssMods(t.chapter) && t.win64 && FullBrightFiles::IsInstalled(*t.win64); }

bool LoadManipInstalled(const Target& t) {
    const auto zip = LoadManipFiles::ZipPath(t.chapter);
    return t.paks && HasFile(zip) && App::Launcher::IsLoadManipInstalled(*t.paks, zip, t.win64, t.chapter);
}

bool StandaloneUe4ssInstalled(const Target& t) {
    return Ue4ssPresent(t) && !App::Launcher::IsUe4ssOwnedByMod(*t.win64, t.chapter);
}

bool ForeignUe4ssBlocksLoadManip(const Target& t) {
    return UsesUe4ssMods(t.chapter) && t.win64 && !FullBrightFiles::IsInstalled(*t.win64) && Ue4ssPresent(t);
}

bool ForeignUe4ssBlocksFullBright(const Target& t) {
    return t.win64 && Ue4ssPresent(t) && !LoadManipFiles::IsUe4ssFromLoadManip(*t.win64) && !FullBrightFiles::IsInstalled(*t.win64);
}

std::wstring GenericUe4ssZip(int chapter) {
    // Chapter 5 runs a newer engine build than Chapters 1-4 and needs its own UE4SS build
    return ModFiles::ToolPath(chapter >= 5 ? L"Chapter 5\\Ue4ss.zip" : L"Chapter 1 - 4\\Ue4ss.zip");
}

// ── UE4SS ──────────────────────────────────────────────────────────────────

Result InstallUe4ss(const Target& t) {
    if (!t.win64) return Fail(kNoGame);
    const std::wstring zip = GenericUe4ssZip(t.chapter);
    if (!Files::Exists(zip)) return Fail(L"UE4SS zip not found. Try restarting the launcher.");
    std::wstring error;
    if (!Extract(zip, *t.win64, error)) return Fail(L"Error installing UE4SS:\n" + error);
    return Ok(L"UE4SS installed successfully!\n\n" + *t.win64);
}

Result RemoveUe4ss(const Target& t) {
    if (!t.win64) return Fail(kNoGame);
    const std::wstring zip = GenericUe4ssZip(t.chapter);
    if (!Files::Exists(zip)) return Fail(L"UE4SS zip not found. Try restarting the launcher.");
    LoadManipFiles::UninstallUe4ss(*t.win64, zip, std::nullopt);
    return Ok(L"UE4SS removed successfully!");
}

Result ReplaceModUe4ssWithStandalone(const Target& t) {
    if (!t.win64) return Fail(kNoGame);
    const std::wstring original = GenericUe4ssZip(t.chapter);
    if (!Files::Exists(original)) return Fail(L"UE4SS zip not found. Try restarting the launcher.");
    // FullBright overwrote Load Manip's files: remove it first so none of its extra files
    // (fullbright.playtime, its UE4SS mods) are left behind
    if (FullBrightInstalled(t) && t.paks) {
        const FullBrightZips z = FullBrightZipsFor(t.chapter);
        if (z.Complete()) UninstallFullBrightKeepConfig(t, z);
    }
    const auto lmZip = LoadManipFiles::ZipPath(t.chapter);
    if (t.paks && HasFile(lmZip)) LoadManipFiles::Uninstall(*t.paks, *lmZip);
    const auto lmUe4ss = LoadManipFiles::Ue4ssZipPath(t.chapter);
    if (HasFile(lmUe4ss)) LoadManipFiles::UninstallUe4ss(*t.win64, *lmUe4ss, LoadManipFiles::MarkerZipPath(t.chapter));
    std::wstring error;
    if (!Extract(original, *t.win64, error)) return Fail(L"Error installing UE4SS:\n" + error);
    return Ok(L"UE4SS installed successfully!\n\n" + *t.win64);
}

// ── Load Manip ─────────────────────────────────────────────────────────────

Result InstallLoadManip(const Target& t, bool deleteForeignUe4ss) {
    if (!t.paks || !t.win64) return Fail(kNoGame);
    const auto zip = LoadManipFiles::ZipPath(t.chapter);
    if (!HasFile(zip)) return Fail(L"Load Manip zip not found. Try restarting the launcher.");
    const auto ue4ssZip = LoadManipFiles::Ue4ssZipPath(t.chapter);
    if (UsesUe4ssMods(t.chapter)) {
        if (!HasFile(ue4ssZip)) return Fail(L"Load Manip UE4SS zip not found. Try restarting the launcher.");
        if (deleteForeignUe4ss) {
            const std::wstring generic = GenericUe4ssZip(t.chapter);
            if (Files::Exists(generic)) LoadManipFiles::UninstallUe4ss(*t.win64, generic, std::nullopt);
        }
    }
    if (!LoadManipFiles::Install(*t.paks, *zip, LoadManipFiles::ConfigZipPath(t.chapter)))
        return Fail(L"Error installing Load Manip files:\nCould not extract the files.");
    if (UsesUe4ssMods(t.chapter) && !LoadManipFiles::InstallUe4ss(*t.win64, *ue4ssZip, LoadManipFiles::MarkerZipPath(t.chapter)))
        return Fail(L"Error installing Load Manip files:\nCould not extract UE4SS.");
    return Ok(L"Load Manip files installed successfully!\n\n" + *t.paks);
}

// Load Manip's pak / UE4SS are the very files FullBright ships, so with FullBright kept, both are
// wiped and FullBright is reinstalled on top (back to a clean FullBright-only state).
Result RemoveLoadManip(const Target& t, bool alsoFullBright) {
    const auto zip = LoadManipFiles::ZipPath(t.chapter);
    if (!t.paks || !HasFile(zip)) return Fail(L"Load Manip zip not found. Try restarting the launcher.");
    const bool fullBright = FullBrightInstalled(t);
    const FullBrightZips z = FullBrightZipsFor(t.chapter);
    const bool canReinstall = t.win64 && fullBright && z.Complete();
    if (canReinstall) UninstallFullBrightKeepConfig(t, z);
    LoadManipFiles::Uninstall(*t.paks, *zip);
    const auto ue4ssZip = LoadManipFiles::Ue4ssZipPath(t.chapter);
    if (t.win64 && HasFile(ue4ssZip) && LoadManipFiles::IsUe4ssFromLoadManip(*t.win64))
        LoadManipFiles::UninstallUe4ss(*t.win64, *ue4ssZip, LoadManipFiles::MarkerZipPath(t.chapter));
    if (canReinstall && !alsoFullBright) FullBrightFiles::Install(*t.paks, *t.win64, *z.pak, *z.ue4ss, *z.marker, z.config);
    if (fullBright && alsoFullBright) return Ok(L"Load Manip and FullBright files removed successfully!");
    if (fullBright) return Ok(L"Load Manip files removed \x2014 FullBright kept installed!");
    return Ok(L"Load Manip files removed successfully!");
}

// ── FullBright ─────────────────────────────────────────────────────────────

Result InstallFullBright(const Target& t, bool withLoadManip, bool deleteForeignUe4ss) {
    if (!t.paks || !t.win64) return Fail(kNoGame);
    const FullBrightZips z = FullBrightZipsFor(t.chapter);
    if (!z.Complete()) return Fail(L"FullBright zip not found. Try restarting the launcher.");
    if (deleteForeignUe4ss) {
        const std::wstring generic = GenericUe4ssZip(t.chapter);
        if (Files::Exists(generic)) LoadManipFiles::UninstallUe4ss(*t.win64, generic, std::nullopt);
    }
    if (withLoadManip && !LoadManipInstalled(t)) {
        // Load Manip goes underneath FullBright (pak + its UE4SS build + launcher.playtime)
        const auto lmZip = LoadManipFiles::ZipPath(t.chapter), lmUe4ss = LoadManipFiles::Ue4ssZipPath(t.chapter);
        if (!HasFile(lmZip) || !HasFile(lmUe4ss)) return Fail(L"Load Manip zip not found. Try restarting the launcher.");
        if (!LoadManipFiles::Install(*t.paks, *lmZip, LoadManipFiles::ConfigZipPath(t.chapter)) ||
            !LoadManipFiles::InstallUe4ss(*t.win64, *lmUe4ss, LoadManipFiles::MarkerZipPath(t.chapter)))
            return Fail(L"Error installing Load Manip files:\nCould not extract the files.");
    }
    if (!FullBrightFiles::Install(*t.paks, *t.win64, *z.pak, *z.ue4ss, *z.marker, z.config))
        return Fail(L"Error installing FullBright files:\nCould not extract the files.");
    return Ok(L"FullBright files installed successfully!\n\n" + *t.paks);
}

// FullBright overwrote Load Manip's pak / UE4SS: when Load Manip was underneath (its own marker
// survives), its base files are restored — or removed too with DELETE ALL.
Result RemoveFullBright(const Target& t, bool alsoLoadManip) {
    const FullBrightZips z = FullBrightZipsFor(t.chapter);
    if (!t.paks || !t.win64 || !z.Complete()) return Fail(L"FullBright zip not found. Try restarting the launcher.");
    const bool loadManipWasInstalled = LoadManipMarker(t);
    UninstallFullBrightKeepConfig(t, z);
    if (loadManipWasInstalled) {
        const auto lmZip = LoadManipFiles::ZipPath(t.chapter), lmUe4ss = LoadManipFiles::Ue4ssZipPath(t.chapter);
        const auto lmMarker = LoadManipFiles::MarkerZipPath(t.chapter);
        if (alsoLoadManip) {
            if (HasFile(lmZip)) LoadManipFiles::Uninstall(*t.paks, *lmZip);
            if (HasFile(lmUe4ss)) LoadManipFiles::UninstallUe4ss(*t.win64, *lmUe4ss, lmMarker);
        } else if (HasFile(lmZip) && HasFile(lmUe4ss)) {
            LoadManipFiles::Install(*t.paks, *lmZip, LoadManipFiles::ConfigZipPath(t.chapter));
            LoadManipFiles::InstallUe4ss(*t.win64, *lmUe4ss, lmMarker);
        }
    }
    if (loadManipWasInstalled && alsoLoadManip) return Ok(L"FullBright and Load Manip files removed successfully!");
    if (loadManipWasInstalled) return Ok(L"FullBright files removed and Load Manip restored successfully!");
    return Ok(L"FullBright files removed successfully!");
}

} // namespace SpeedrunLauncher::Services::ModInstaller
