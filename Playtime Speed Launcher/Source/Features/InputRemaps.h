#pragma once
#include <memory>
#include <set>
#include "Services/Hotkeys/CoresService.h"

namespace SpeedrunLauncher::Main {
class MainWindow;
}

namespace SpeedrunLauncher::Features {

// Global keyboard / mouse features built on the shared low-level hooks:
//  - Cores hotkeys (Freeze / Slower / Normal) while a game is in the foreground
//  - Chapter 4 load remap: user keys / mouse buttons → the fixed in-game I / O / P keys
//  - feeding key presses to the easter eggs while the game has focus
//  - key → click: chosen keys become mouse clicks (optionally spamming while held) in-game
class InputRemaps {
public:
    explicit InputRemaps(Main::MainWindow& window) : main(window) {}
    ~InputRemaps() { Shutdown(); }

    void Initialize();          // installs whatever the saved settings enable
    void Shutdown();

    void RefreshCores();        // after the enable toggle / key bindings change
    void RefreshChapter4();
    void RefreshEasterEggHook();
    void RefreshKeyClicks();    // after the key → click settings change
    // True while any keyboard hook runs (window key events are then unreliable for the easter eggs)
    bool AnyKeyboardHookActive() const;

    void HandleCoresHotkey(Services::CoresMode mode);

    // Every real key press seen by a hook (dedupes the same press seen twice). Returns true when the
    // key must be swallowed (Escape closing an easter egg video over the game).
    bool FeedEasterEgg(unsigned vk, unsigned time);

private:
    bool Chapter4Active() const;
    void ShowCoresToast(Services::CoresMode mode, const std::wstring& processName, int chapter);

    Main::MainWindow& main;
    void StopKeyClicks();

    int coresKeyboard = 0, chapter4Keyboard = 0, chapter4Mouse = 0, eggKeyboard = 0, keyClickKeyboard = 0;
    std::shared_ptr<struct KeyClickState> keyClickState;   // shared with the hook thread / spam threads
    unsigned lastEggVk = 0, lastEggTime = 0;
};

} // namespace SpeedrunLauncher::Features
