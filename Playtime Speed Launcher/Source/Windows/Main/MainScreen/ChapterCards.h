#pragma once
#include "App/Launcher.h"
#include "Graphics/Images.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// The shelf at the bottom of the main screen: the Chapters / Multiplayers tabs and, under them, a
// row of small poster cards (banner, title, playtime or status, "coming soon" lock). Clicking one
// selects it (the hero above shows it); the mouse wheel scrolls the row sideways; switching tabs
// slides the new cards in.
class ChapterCards {
public:
    explicit ChapterCards(MainWindow& window) : main(window) {}
    void Draw(UI::Ui& ui, const Rect& area);

    // Chapters' banner (jpg, else png), resolved once per chapter
    static const std::wstring& BannerPath(int chapter);

private:
    // What one card shows (a chapter or a multiplayer game)
    struct Card {
        std::wstring banner;
        std::wstring title;
        std::wstring subtitle;   // playtime / status
        bool available = true;
        bool selected = false;
        bool running = false;
        bool dimmed = true;      // unselected cards are dimmed (not while nothing is picked)
        Font font = Font::Mono;   // chapters: the Poppy Playtime font
    };

    void DrawTabs(UI::Ui& ui, const Rect& area);
    void DrawCards(UI::Ui& ui, const Rect& area);
    void DrawCard(UI::Ui& ui, const Card& card, const Rect& r, float hover);

    MainWindow& main;
    App::Launcher::Section shownSection = App::Launcher::Section::Chapters;
    double sectionChangedAt = -10.0;
};

} // namespace SpeedrunLauncher::Main
