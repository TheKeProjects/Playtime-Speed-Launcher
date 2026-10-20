#include "Features/EasterEggs.h"
#include <windows.h>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Windows/GameOverlays/EasterEggOverlayWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/EasterEgg/EasterEggOverlay.h"

namespace SpeedrunLauncher::Features {

EasterEggs::EasterEggs(Main::MainWindow& window) : main(window) {}

EasterEggs::~EasterEggs() = default;

void EasterEggs::HandleKey(unsigned vk) {
    if (vk == VK_ESCAPE && playing) {
        Hide();
        return;
    }
    if (auto video = sequences.Feed(vk)) Show(*video);
}

void EasterEggs::Show(const std::wstring& fileName) {
    if (playing) return;
    const std::wstring path = Paths::Asset(L"Videos\\EasterEggs\\" + fileName);
    if (!Files::Exists(path)) return;
    const std::wstring ext = Text::Lower(Files::Extension(path));
    const bool photo = ext == L".png" || ext == L".jpg" || ext == L".jpeg";
    playing = true;
    if (App::L().IsGameForeground()) {
        gameWindow = std::make_unique<Windows::EasterEggOverlayWindow>(path, photo);
        gameWindow->onFinished = [this] { Hide(); };
        if (!gameWindow->Open()) Hide();
        return;
    }
    main.EggVideo().Play(path, photo);
}

void EasterEggs::Hide() {
    if (gameWindow) {
        auto window = std::move(gameWindow);
        window->onFinished = nullptr;
        window->Destroy();
    }
    main.EggVideo().Stop();
    playing = false;
}

} // namespace SpeedrunLauncher::Features
