#pragma once
#include "Core/Async.h"

namespace SpeedrunLauncher::Main {
class MainWindow;
}

namespace SpeedrunLauncher::Features {

// Every 2 seconds: which chapter's game is running (prefers the *-Win64-Shipping renderer over a
// launcher stub), adds playtime, shows the "game started" toasts, starts / stops FPS tracking,
// keeps Discord and the info panel in sync and restores the UE4SS hotkey remap on exit.
class GameWatcher {
public:
    static constexpr unsigned IntervalMs = 2000;

    explicit GameWatcher(Main::MainWindow& window) : main(window) {}
    ~GameWatcher() { Stop(); }
    void Start();
    void Stop();
    void Tick();

private:
    Main::MainWindow& main;
    Async::TimerId timer = 0;
    bool initialized = false;
};

} // namespace SpeedrunLauncher::Features
