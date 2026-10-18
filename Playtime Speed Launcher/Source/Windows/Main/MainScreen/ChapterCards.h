#pragma once
#include "App/Launcher.h"
#include "Graphics/Images.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// The Chapters / Multiplayers tabs and, under them, the horizontal strip of cards (banner, title,
// playtime or status, Save / UE4SS / MANIP / FULLBRIGHT buttons, "coming soon" lock). Mouse wheel
// scrolls it sideways; switching tabs slides the new cards in.
class ChapterCards {
public:
    explicit ChapterCards(MainWindow& window) : main(window) {}
    void Draw(UI::Ui& ui, const Rect& area);

private:
    // What one card shows (a chapter or a multiplayer game)
    struct Card {
        std::wstring banner;
        std::wstring title;
        std::wstring subtitle;   // playtime / status
        bool available = true;
        bool selected = false;
        int chapterIndex = -1;   // chapters only: the Save / mod buttons
    };

    void DrawTabs(UI::Ui& ui, const Rect& area);
    void DrawCards(UI::Ui& ui, const Rect& area);
    void DrawCard(UI::Ui& ui, const Card& card, int index, const Rect& r, float opacity);
    void DrawCardButtons(UI::Ui& ui, size_t index, float x, float y);

    MainWindow& main;
    App::Launcher::Section shownSection = App::Launcher::Section::Chapters;
    double sectionChangedAt = -10.0;
};

} // namespace SpeedrunLauncher::Main
