#include "Windows/Main/Overlays/Tutorials/TutorialPopups.h"
#include "App/Paths.h"
#include "Localization/Localization.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

void TutorialPopups::ShowVideoPopup() {
    if (video) video->Destroy();
    video = std::make_unique<Windows::TutorialVideoWindow>();
    auto* raw = video.get();
    video->onClosed = [this, raw] {
        if (video.get() == raw) video.reset();
    };
    if (!video->Open(main.Hwnd())) video.reset();
}

void TutorialPopups::ShowSteamTutorial() {
    const auto image = [](const wchar_t* file) { return Paths::Asset(std::wstring(L"Images\\Tutorial\\") + file); };
    std::vector<Windows::TutorialStepsWindow::Step> list{
        { image(L"1.png"), std::nullopt, false, false },
        { image(L"2.png"), Loc::Get("steam_tutorial_paste_here"), true, false },
        { image(L"3.png"), std::nullopt, false, false },
        { std::nullopt, Loc::Get("controller_setup_restart"), false, true },
    };
    if (steps) steps->Destroy();
    steps = std::make_unique<Windows::TutorialStepsWindow>();
    auto* raw = steps.get();
    steps->onClosed = [this, raw] {
        if (steps.get() == raw) steps.reset();
    };
    if (!steps->Open(main.Hwnd(), std::move(list))) steps.reset();
}

} // namespace SpeedrunLauncher::Main
