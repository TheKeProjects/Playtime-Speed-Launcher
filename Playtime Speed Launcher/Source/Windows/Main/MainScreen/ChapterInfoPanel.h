#pragma once
#include <string>
#include "Seasonal/SeasonalTheme.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// Title, status, version button, Steam/Epic toggle and description of the selected chapter.
// Refresh() recomputes the cached texts (it checks files and processes, so not every frame).
class ChapterInfoPanel {
public:
    explicit ChapterInfoPanel(MainWindow& window) : main(window) {}
    void Refresh();
    float Height(UI::Ui& ui, float width);
    void Draw(UI::Ui& ui, const Rect& area);

    bool CanPlay() const { return canPlay; }
    bool Running() const { return running; }

private:
    void DrawContent(UI::Ui& ui, const Rect& area, const Seasonal::Palette& palette, bool seasonal);

    MainWindow& main;
    std::wstring title, description, status, version;
    std::wstring shownTitle;          // title of the last drawn chapter (a change starts the transition)
    double changedAt = -10.0;
    bool canPlay = false;
    bool running = false;
};

} // namespace SpeedrunLauncher::Main
