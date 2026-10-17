#pragma once
#include "Graphics/Images.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// The horizontal strip of chapter cards (banner, title, playtime, Save / UE4SS / MANIP /
// FULLBRIGHT buttons, "coming soon" lock). Mouse wheel scrolls it sideways.
class ChapterCards {
public:
    explicit ChapterCards(MainWindow& window) : main(window) {}
    void Draw(UI::Ui& ui, const Rect& area);

private:
    void DrawCard(UI::Ui& ui, size_t index, const Rect& r, bool selected, float opacity);
    void DrawCardButtons(UI::Ui& ui, size_t index, float x, float y);

    MainWindow& main;
};

} // namespace SpeedrunLauncher::Main
