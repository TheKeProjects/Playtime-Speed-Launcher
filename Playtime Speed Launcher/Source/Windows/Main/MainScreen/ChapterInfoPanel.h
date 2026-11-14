#pragma once
#include <string>
#include <vector>
#include "Seasonal/SeasonalTheme.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// The hero of the main screen: the selected chapter (or multiplayer game) big. On the right, a
// carousel of the developers' own screenshots (crossfading on their own, with dots under it: the
// current one a pill that fills up until the next slide; the poster when there are none); on the left its title, status and playtime chips,
// description, version picker and Steam / Epic switch, the PLAY button and the chapter's tools
// (checkpoint saves, UE4SS, Load Manip, FullBright).
// Refresh() recomputes the cached texts (it checks files and processes, so not every frame).
class ChapterInfoPanel {
public:
    explicit ChapterInfoPanel(MainWindow& window) : main(window) {}
    void Refresh();
    void Draw(UI::Ui& ui, const Rect& area);
    // Classic theme only (the seasonal ones have their scenery): the current screenshot (or poster)
    // big and dark behind the whole content area, so the hero doesn't float on a flat background.
    void DrawBackdrop(UI::Ui& ui, const Rect& area);

    bool CanPlay() const { return canPlay; }
    bool Running() const { return running; }

private:
    void DrawArt(UI::Ui& ui, const Rect& area, const Rect& poster, float enter);
    void DrawCarousel(UI::Ui& ui, const Rect& frame, float enter);
    void DrawLock(UI::Ui& ui, const Rect& r);
    void DrawPrompt(UI::Ui& ui, const Rect& area);   // nothing picked yet: "Pick a chapter"
    float ContentHeight(UI::Ui& ui, float width);
    void DrawContent(UI::Ui& ui, float x, float y, float width);
    void DrawVersionRow(UI::Ui& ui, float x, float y);
    void DrawPlay(UI::Ui& ui, float x, float y);
    void DrawTools(UI::Ui& ui, float x, float y);

    MainWindow& main;
    std::wstring title, description, status, version, eyebrow, banner;
    enum class State { Playing, Installed, NotFound, ComingSoon } state = State::NotFound;
    std::vector<std::wstring> screenshots;   // Assets\Screenshots\<chapter or game>\1..5.jpg
    int slide = 0, previousSlide = -1;
    double slideStartedAt = 0.0;      // moved forward while paused (hover, resting) so progress holds
    double slideChangedAt = -10.0;    // start of the crossfade from previousSlide
    std::wstring shownTitle;          // title of the last drawn chapter (a change starts the transition)
    double changedAt = -10.0;
    bool canPlay = false;
    bool running = false;
    bool multiplayer = false;
    bool mpVersions = false;          // multiplayer game with old versions (Escape From Playtime): VERSION button
    bool picked = false;              // false until a card of the current tab is clicked
    int variant = 0;                  // seasonal poster decorations: stable per chapter / game
    Font titleFont = Font::Mono;      // chapters: the Poppy Playtime font; multiplayer games: the launcher's
    Font bodyFont = Font::Mono;
};

} // namespace SpeedrunLauncher::Main
