#pragma once
#include <algorithm>
#include "Graphics/Rect.h"

namespace SpeedrunLauncher::Main {

// The main screen grid:
//   ┌──────────────── top bar (36) ────────────────┐
//   │ rail │ hero: selected chapter (title, PLAY,   │
//   │ (88) │ tools, poster)                         │
//   │      ├────────────────────────────────────────┤
//   │      │ shelf: Chapters / Multiplayers + cards │
//   └──────┴────────────────────────────────────────┘
struct MainLayout {
    Rect screen;
    Rect topBar;
    Rect rail;
    Rect hero;
    Rect shelf;
    Rect content;   // hero + shelf

    static constexpr float TopBarHeight = 36.f;
    static constexpr float RailWidth = 88.f;
    static constexpr float ShelfHeight = 336.f;
    // Left / right margin shared by the hero and the shelf, so the title, the tabs and the cards line up
    static constexpr float ContentPad = 56.f;

    static MainLayout Compute(const Rect& screen) {
        MainLayout l;
        l.screen = screen;
        l.topBar = screen.Top(TopBarHeight);
        const Rect body = screen.CutTop(TopBarHeight);
        l.rail = body.Left(RailWidth);
        const Rect content = body.CutLeft(RailWidth);
        l.content = content;
        // Small windows: the shelf gives up a little room so the hero still fits
        const float shelfH = std::clamp(content.h * 0.34f, 300.f, ShelfHeight);
        l.shelf = content.Bottom(shelfH);
        l.hero = content.CutBottom(shelfH);
        return l;
    }
};

} // namespace SpeedrunLauncher::Main
