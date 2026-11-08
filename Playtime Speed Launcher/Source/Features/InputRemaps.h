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
//  - feeding key presses to the easter eggs while the game has focus
//  - key → click: chosen keys become mouse clicks (optionally spamming while held) in-game
//  - 100%: a key (default Backspace) that puts every chapter's collectibles back to 0, in-game
class InputRemaps {
public:
    explicit InputRemaps(Main::MainWindow& window) : main(window) {}
    ~InputRemaps() { Shutdown(); }

    void Initialize();          // installs whatever the saved settings enable
    void Shutdown();

    void RefreshCores();        // after the enable toggle / key bindings change
    void RefreshEasterEggHook();
    void RefreshKeyClicks();    // after the key → click settings change
    void RefreshCollectiblesReset();   // after the 100% reset key changes
    void ResetCollectibles();   // every chapter's 100% progress back to 0 (UI thread)
    // True while any keyboard hook runs (window key events are then unreliable for the easter eggs)
    bool AnyKeyboardHookActive() const;

    void HandleCoresHotkey(Services::CoresMode mode);

    // Every real key press seen by a hook (dedupes the same press seen twice). Returns true when the
    // key must be swallowed (Escape closing an easter egg video over the game).
    bool FeedEasterEgg(unsigned vk, unsigned time);

private:
    void ShowCoresToast(Services::CoresMode mode, const std::wstring& processName, int chapter);

    Main::MainWindow& main;
    void StopKeyClicks();

    int coresKeyboard = 0, eggKeyboard = 0, keyClickKeyboard = 0, collectResetKeyboard = 0;
    std::shared_ptr<struct KeyClickState> keyClickState;   // shared with the hook thread / spam threads
    unsigned lastEggVk = 0, lastEggTime = 0;
};

} // namespace SpeedrunLauncher::Features
