#pragma once

// 100% in game: shows the collectibles overlay over the game (Windows\GameOverlays\CollectiblesOverlayWindow)
// while a chapter is running, when Settings → 100% → "Over the game" is on. Only while the game is the
// active window: minimized or switched away from, the overlay hides (at once, through WinEvent hooks). It's the same page as the
// OBS link, with the same options (Settings::Collectibles). UI thread only.
namespace SpeedrunLauncher::Services::Collectibles::InGameOverlay {

void Start();      // starts watching for the game (every 2 s)
void Shutdown();   // closes the window and stops watching

// After Settings::Collectibles changed: shows / hides / reloads the window at once
void Apply();
// Shows it for a few seconds even without the game, to see how it looks
void Preview();
bool Showing();    // the window is up right now

// Move mode: the overlay shows (game or not), takes the mouse and can be dragged up and down; the
// height is saved as it's dropped. EndMove puts it back to normal (click-through).
void StartMove();
void EndMove();
bool Moving();
void ResetPosition();   // back to the top of the screen

} // namespace SpeedrunLauncher::Services::Collectibles::InGameOverlay
