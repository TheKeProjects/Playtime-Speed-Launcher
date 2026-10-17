#pragma once

// The separate top-level windows the launcher opens over the game / desktop.
namespace SpeedrunLauncher::Standalone {

void ToggleCheckpointOverlay();       // HotkeyOverlay (global hotkey, default Ctrl+Shift+Enter)
void ToggleVideoTutorialOverlay();    // VideoTutorialOverlay (global hotkey, default F9)
void ToggleLeaderboard();             // LeaderboardOverlay
void ShowBeginnerTutorials();         // BeginnerTutorialOverlay
void OpenManualUpdate();              // ManualUpdateWindow
void CloseAll();

} // namespace SpeedrunLauncher::Standalone
