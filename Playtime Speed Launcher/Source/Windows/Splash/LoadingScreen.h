#pragma once

// Loading screen shown before the main window: one random chapter picture and a progress bar while
// the embedded assets are unpacked, the translations and the Poppy font are loaded and the main
// screen's pictures are decoded. Runs its own message loop and returns once everything is ready.
namespace SpeedrunLauncher::Splash {

void RunLoadingScreen();

} // namespace SpeedrunLauncher::Splash
