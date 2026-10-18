#include "Features/GameWatcher.h"
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Features/GameOverlays.h"
#include "Features/GlobalHotkeys.h"
#include "Features/InputRemaps.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Features {

void GameWatcher::Start() {
    if (!timer) timer = Async::Every(IntervalMs, [this] { Tick(); });
}

void GameWatcher::Stop() {
    if (timer) Async::CancelTimer(timer);
    timer = 0;
}

void GameWatcher::Tick() {
    auto& l = App::L();
    bool anyRunning = false, stateChanged = false;
    // One snapshot of the process table for every lookup (this runs on the UI thread)
    const auto processes = Process::List();
    for (size_t i = 0; i < l.chapters.size(); ++i) {
        auto path = l.ActiveExePath(l.chapters[i]);
        if (!path || path->empty()) {
            l.gameRunning[i] = false;
            l.runningPid[i] = 0;
            continue;
        }
        // Some installs point at a thin UE launcher stub that stays resident next to the real
        // renderer it spawns: FPS tracking must follow the shipping process when there is one.
        const auto stubPids = Process::FindByName(processes, Files::FileNameWithoutExtension(*path));
        std::vector<uint32_t> shippingPids;
        if (auto shipping = l.ShippingExeName(*path)) shippingPids = Process::FindByName(processes, *shipping);
        const bool running = !stubPids.empty() || !shippingPids.empty();
        l.runningPid[i] = running ? (!shippingPids.empty() ? shippingPids[0] : stubPids[0]) : 0;

        if (initialized && running && !l.gameRunning[i]) {
            main.ShowGameToast();
            main.ShowTutorialToast();
        }
        if (running && !l.gameRunning[i]) main.GameOverlays().StartFps(l.runningPid[i]);
        else if (!running && l.gameRunning[i]) main.GameOverlays().StopFps();

        if (running) l.playtime.AddSeconds(l.chapters[i].number, IntervalMs / 1000.0);

        if (running != static_cast<bool>(l.gameRunning[i])) stateChanged = true;
        l.gameRunning[i] = running;
        if (running) anyRunning = true;
    }
    l.PublishRunningGames();   // for the input hook thread

    // Multiplayer games: only the PLAYING status and the resting scenery follow them
    for (size_t i = 0; i < l.multiplayers.size(); ++i) {
        const auto& exe = l.multiplayers[i].gameExePath;
        const bool running = exe && !Process::FindByName(processes, Files::FileNameWithoutExtension(*exe)).empty();
        if (running != static_cast<bool>(l.multiplayerRunning[i])) stateChanged = true;
        l.multiplayerRunning[i] = running;
    }

    // Discord always reflects what's selected in the launcher once the game closes
    if (initialized && !anyRunning && stateChanged && l.discord)
        l.discord->SetChapterSelected(l.Selected(), l.VersionLabel(l.Selected()));
    if (stateChanged) main.RefreshInfo();

    main.Remaps().RefreshEasterEggHook();

    // Restore the UE4SS hotkey remap once the game it was applied for exits
    if (initialized) main.Hotkeys().RestoreIfGameExited();
    initialized = true;
    main.Invalidate();   // playtime texts on the cards
}

} // namespace SpeedrunLauncher::Features
