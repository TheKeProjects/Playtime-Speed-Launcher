#pragma once
#include <array>
#include <string>
#include "UI/Window.h"
#include "Windows/Standalone/Common/VideoPlayerPanel.h"

namespace SpeedrunLauncher::Windows {

// "BEGINNER TUTORIALS" (the TUTORIALS button of the bottom bar), following the seasonal theme:
// a path of four numbered steps with a progress bar of the ones already watched (remembered in
// tutorials_watched.cfg) and a card per video (YouTube thumbnail, title, what it teaches). Picking
// one plays it in the embedded player with the four steps listed beside it and a "next" button.
class BeginnerTutorialWindow : public UI::Window {
public:
    ~BeginnerTutorialWindow() override { player.Destroy(); }
    bool Open();

protected:
    void OnFrame() override;
    void OnClosing(bool& cancel) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void DrawPath(const Rect& area);
    void DrawCards(const Rect& area);
    void DrawPlaylist(const Rect& area);
    void Play(int index);
    void ShowCards();
    void BeginClose();
    void SaveWatched() const;
    int WatchedCount() const;

    VideoPlayerPanel player;
    int current = -1;                  // playing card, -1 = cards view
    std::array<bool, 4> watched{};     // opened at least once (persisted)
    double openedAt = -10.0;           // panel pop-in
    double cardsShownAt = -10.0;       // cards slide in one by one (after opening / coming back from a video)
    double viewChangedAt = -10.0;      // cards ↔ video cross-fade
    double closingAt = -1.0;           // fade-out running (the window closes when it ends)
    bool finished = false;
};

} // namespace SpeedrunLauncher::Windows
