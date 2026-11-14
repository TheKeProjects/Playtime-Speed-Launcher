#include "Services/GameFiles/CustomNameFiles.h"
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/GameFiles/ModFiles.h"

namespace SpeedrunLauncher::Services::CustomNameFiles {

namespace {

std::wstring ModDir(const std::wstring& win64) { return Files::Combine(win64, L"ue4ss", L"Mods", ModFolder); }
std::wstring ConfigPath(const std::wstring& win64) { return Files::Combine(ModDir(win64), L"config.ini"); }
std::wstring ScriptSource() { return ModFiles::ToolPath(L"EFP Custom Name\\main.lua"); }
// Escape From Playtime is UE 5.6 like Chapter 5: its UE4SS build (with the 5.6 override) fits
std::wstring Ue4ssZip() { return ModFiles::ToolPath(L"Chapter 5\\Ue4ss.zip"); }

// Our own UE4SS: no debug window over the game, and only the mods we need
void TuneOwnUe4ss(const std::wstring& win64) {
    const std::wstring ini = Files::Combine(win64, L"ue4ss", L"UE4SS-settings.ini");
    ModFiles::UpdateConfigKey(ini, L"GuiConsoleEnabled", L"0");
    ModFiles::UpdateConfigKey(ini, L"GuiConsoleVisible", L"0");
    ModFiles::UpdateConfigKey(ini, L"ConsoleEnabled", L"0");
    Files::WriteText(Files::Combine(win64, L"ue4ss", L"Mods", L"mods.txt"),
                     std::wstring(ModFolder) + L" : 1\r\nBPML_GenericFunctions : 1\r\nBPModLoaderMod : 1\r\n\r\n; Built-in keybinds, do not move up!\r\nKeybinds : 1\r\n");
}

} // namespace

std::optional<std::wstring> Win64ForExe(const std::wstring& exePath) {
    if (exePath.empty()) return std::nullopt;
    return App::Launcher::FindWin64Dir(Files::DirectoryName(exePath));
}

bool IsInstalled(const std::wstring& win64Dir) { return Files::Exists(Files::Combine(ModDir(win64Dir), L"Scripts", L"main.lua")); }

bool WriteConfig(const std::wstring& win64Dir, const std::wstring& config) {
    if (!Files::DirectoryExists(ModDir(win64Dir))) return false;
    return Files::WriteText(ConfigPath(win64Dir), config);
}

Result Install(const std::wstring& win64Dir, const std::wstring& config) {
    if (!Files::Exists(ScriptSource())) return { false, L"The Custom Name files are missing from the launcher.\nRestart the launcher and try again." };
    if (!App::Launcher::IsUe4ssInstalled(win64Dir)) {
        if (!Files::Exists(Ue4ssZip())) return { false, L"The UE4SS files are missing from the launcher.\nRestart the launcher and try again." };
        if (!Zip::ExtractToDirectory(Ue4ssZip(), win64Dir, true)) return { false, L"Could not add UE4SS to this version.\nClose the game and try again." };
        Files::WriteText(Files::Combine(win64Dir, Marker), L"");
        TuneOwnUe4ss(win64Dir);
    }
    const std::wstring mod = ModDir(win64Dir);
    Files::CreateDirectories(Files::Combine(mod, L"Scripts"));
    if (!Files::Copy(ScriptSource(), Files::Combine(mod, L"Scripts", L"main.lua"), true))
        return { false, L"Could not copy the Custom Name files.\nClose the game and try again." };
    Files::WriteText(Files::Combine(mod, L"enabled.txt"), L"");   // UE4SS loads mods with this file without touching mods.txt
    Files::WriteText(ConfigPath(win64Dir), config);
    return { true, L"Custom Name added to this version!\nYour new name shows up the next time you start the game." };
}

Result Uninstall(const std::wstring& win64Dir) {
    const std::wstring mod = ModDir(win64Dir);
    if (Files::DirectoryExists(mod) && !Files::DeleteDirectory(mod)) return { false, L"Could not remove the Custom Name files.\nClose the game and try again." };
    // The UE4SS we brought goes too
    if (Files::Exists(Files::Combine(win64Dir, Marker))) {
        if (Files::Exists(Ue4ssZip())) ModFiles::DeleteZipContents(win64Dir, Ue4ssZip());
        const std::wstring ue4ss = Files::Combine(win64Dir, L"ue4ss");
        if (Files::DirectoryExists(ue4ss)) Files::DeleteDirectory(ue4ss);   // logs / caches UE4SS wrote while running
        Files::Delete(Files::Combine(win64Dir, Marker));
    }
    return { true, L"Custom Name removed from this version.\nThe game uses its own name again." };
}

void SyncAll(const std::vector<std::wstring>& exePaths, const std::wstring& config) {
    const auto latest = Files::ReadBytes(ScriptSource());
    for (const auto& exe : exePaths) {
        auto win64 = Win64ForExe(exe);
        if (!win64 || !IsInstalled(*win64)) continue;
        // A launcher update may bring a newer script: the version gets it too
        const std::wstring script = Files::Combine(ModDir(*win64), L"Scripts", L"main.lua");
        if (latest && Files::ReadBytes(script) != latest) Files::WriteBytes(script, *latest);
        WriteConfig(*win64, config);
    }
}

} // namespace SpeedrunLauncher::Services::CustomNameFiles
