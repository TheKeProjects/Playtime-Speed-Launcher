#include "Windows/Standalone/StandaloneWindows.h"
#include <memory>
#include "App/Launcher.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Standalone/BeginnerTutorialWindow.h"
#include "Windows/Standalone/CheckpointOverlayWindow.h"
#include "Windows/Standalone/LeaderboardWindow.h"
#include "Windows/Standalone/ManualUpdateWindow.h"
#include "Windows/Standalone/VideoTutorialWindow.h"

namespace SpeedrunLauncher::Standalone {

namespace {

std::unique_ptr<Windows::CheckpointOverlayWindow> checkpoint;
std::unique_ptr<Windows::VideoTutorialWindow> videoTutorial;
std::unique_ptr<Windows::LeaderboardWindow> leaderboard;
std::unique_ptr<Windows::BeginnerTutorialWindow> beginner;
std::unique_ptr<Windows::ManualUpdateWindow> manualUpdate;

// Closes the window when it's open, otherwise creates it. `onClose` runs after it closed.
template <typename W, typename OpenFn>
void Toggle(std::unique_ptr<W>& slot, OpenFn open, std::function<void()> onClose = {}) {
    if (slot && slot->IsOpen()) {
        slot->Close();
        return;
    }
    slot = std::make_unique<W>();
    W* raw = slot.get();
    slot->onClosed = [&slot, raw, onClose] {
        if (slot.get() == raw) slot.reset();
        if (onClose) onClose();
    };
    if (!open(*slot)) slot.reset();
}

HWND MainHwnd() {
    auto* main = Main::MainWindow::Instance();
    return main ? main->Hwnd() : nullptr;
}

} // namespace

void ToggleCheckpointOverlay() {
    auto& l = App::L();
    if (!(checkpoint && checkpoint->IsOpen())) {
        if (auto* main = Main::MainWindow::Instance(); main && main->IntroPlaying()) main->HideIntro();
        if (const auto* running = l.RunningChapter()) l.discord->SetSelectingCheckpoint(*running, l.VersionLabel(*running));
    }
    Toggle(checkpoint, [](Windows::CheckpointOverlayWindow& w) { return w.Open(); }, [] {
        auto& launcher = App::L();
        if (const auto* running = launcher.RunningChapter()) launcher.discord->SetGameRunning(*running, launcher.VersionLabel(*running));
    });
}

void ToggleVideoTutorialOverlay() {
    Toggle(videoTutorial, [](Windows::VideoTutorialWindow& w) { return w.Open(); }, [] {
        auto& launcher = App::L();
        if (!launcher.chapters.empty()) launcher.discord->SetChapterSelected(launcher.Selected(), launcher.VersionLabel(launcher.Selected()));
        else launcher.discord->SetBrowsing();
    });
}

void ToggleLeaderboard() {
    Toggle(leaderboard, [](Windows::LeaderboardWindow& w) { return w.Open(); });
}

void ShowBeginnerTutorials() {
    Toggle(beginner, [](Windows::BeginnerTutorialWindow& w) { return w.Open(); });
}

void OpenManualUpdate() {
    if (manualUpdate && manualUpdate->IsOpen()) return;
    Toggle(manualUpdate, [](Windows::ManualUpdateWindow& w) { return w.Open(MainHwnd()); });
}

void CloseAll() {
    for (UI::Window* w : std::initializer_list<UI::Window*>{ checkpoint.get(), videoTutorial.get(), leaderboard.get(), beginner.get(),
                                                                      manualUpdate.get() })
        if (w && w->IsOpen()) w->Destroy();
    checkpoint.reset();
    videoTutorial.reset();
    leaderboard.reset();
    beginner.reset();
    manualUpdate.reset();
}

} // namespace SpeedrunLauncher::Standalone
