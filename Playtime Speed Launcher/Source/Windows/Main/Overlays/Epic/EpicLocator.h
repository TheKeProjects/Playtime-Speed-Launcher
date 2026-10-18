#pragma once
#include <windows.h>
#include <functional>

// Finds the Epic Games install of the chapters: a live "searching" dialog while the drives are
// scanned on a worker (the launcher stays responsive), then a "not found" dialog to pick the
// folder by hand. `located` runs on the UI thread once a base path has been stored.
namespace SpeedrunLauncher::Main::EpicLocator {

void Start(HWND owner, std::function<void()> located);

} // namespace SpeedrunLauncher::Main::EpicLocator
