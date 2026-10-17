#pragma once

namespace SpeedrunLauncher::App {

// --steam-shortcut-helper <exePath> <iconSourcePath>
// A separate process (never the one Steam tracks) closes Steam, edits shortcuts.vdf and restarts
// Steam, asking for confirmation first and reporting the result in a dialog.
inline constexpr const wchar_t* SteamShortcutHelperArg = L"--steam-shortcut-helper";
int RunSteamShortcutHelper(int argc, wchar_t** argv);

} // namespace SpeedrunLauncher::App
