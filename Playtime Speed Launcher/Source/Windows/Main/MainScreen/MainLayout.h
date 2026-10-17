#pragma once
#include "Graphics/Rect.h"

namespace SpeedrunLauncher::Main {

// The main screen grid (MainWindow.xaml rows): 36 px status bar, chapter cards (*), chapter info
// (auto), action buttons (auto) and the 38 px bottom bar.
struct MainLayout {
    Rect screen;
    Rect topBar;
    Rect cards;
    Rect info;
    Rect actions;
    Rect bottomBar;

    static constexpr float TopBarHeight = 36.f;
    static constexpr float BottomBarHeight = 38.f;
    static constexpr float ActionRowHeight = 4.f + 58.f + 14.f;   // Margin="0,4,0,14"

    static MainLayout Compute(const Rect& screen, float infoHeight) {
        MainLayout l;
        l.screen = screen;
        l.topBar = screen.Top(TopBarHeight);
        l.bottomBar = screen.Bottom(BottomBarHeight);
        l.actions = { screen.x, l.bottomBar.y - ActionRowHeight, screen.w, ActionRowHeight };
        l.info = { screen.x, l.actions.y - infoHeight, screen.w, infoHeight };
        l.cards = { screen.x, l.topBar.B(), screen.w, l.info.y - l.topBar.B() };
        return l;
    }
};

} // namespace SpeedrunLauncher::Main
