#pragma once

namespace SpeedrunLauncher::App {

// --steam-shortcut-helper <exePath> <iconSourcePath>
// A separate process (never the one Steam tracks) closes Steam, edits shortcuts.vdf and restarts
// Steam in one window: confirmation (library preview), live steps with a progress bar, then the result.
inline constexpr const wchar_t* SteamShortcutHelperArg = L"--steam-shortcut-helper";
int RunSteamShortcutHelper(int argc, wchar_t** argv);

} // namespace SpeedrunLauncher::App
