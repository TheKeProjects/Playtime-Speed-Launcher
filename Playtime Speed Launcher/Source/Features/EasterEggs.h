#pragma once
#include <memory>
#include <string>
#include "Services/App/EasterEggSequences.h"

namespace SpeedrunLauncher::Main {
class MainWindow;
}

namespace SpeedrunLauncher::Windows {
class EasterEggOverlayWindow;
}

namespace SpeedrunLauncher::Features {

// Key-sequence easter eggs: plays the matching video inside the launcher, or over the game in a
// click-through window when the game has focus.
class EasterEggs {
public:
    explicit EasterEggs(Main::MainWindow& window);
    ~EasterEggs();

    void HandleKey(unsigned vk);
    void Hide();
    bool Playing() const { return playing; }
    bool PlayingInGame() const { return gameWindow != nullptr; }

private:
    void Show(const std::wstring& fileName);

    Main::MainWindow& main;
    Services::EasterEggSequences sequences;
    std::unique_ptr<Windows::EasterEggOverlayWindow> gameWindow;
    bool playing = false;
};

} // namespace SpeedrunLauncher::Features
