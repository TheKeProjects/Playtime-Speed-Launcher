#pragma once
#include <string>
#include "UI/Window.h"
#include "Windows/Standalone/Common/VideoPlayerPanel.h"

namespace SpeedrunLauncher::Windows {

// "BEGINNER TUTORIALS": four numbered video cards (YouTube thumbnails) that slide in one by one;
// picking one plays it in the embedded player with a back button to the cards. The panel pops in
// when opened and fades out when closed.
class BeginnerTutorialWindow : public UI::Window {
public:
    ~BeginnerTutorialWindow() override { player.Destroy(); }
    bool Open();

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void DrawCards(const Rect& area);
    void ShowCards();
    void BeginClose();

    VideoPlayerPanel player;
    int current = -1;            // playing card, -1 = cards view
    double openedAt = -10.0;     // panel pop-in
    double cardsShownAt = -10.0; // cards slide in one by one (after opening / coming back from a video)
    double closingAt = -1.0;     // fade-out running (the window closes when it ends)
};

} // namespace SpeedrunLauncher::Windows
