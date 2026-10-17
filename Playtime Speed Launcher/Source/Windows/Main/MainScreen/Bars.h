#pragma once
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

namespace TopBar {
// "Approved by SPEEDRUN.COM", online users counter, minimize button (windowed mode), version.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace TopBar

namespace ActionBar {
// Hand mods · leaderboard · settings (+ update badge) | PLAY CHAPTER | quit.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace ActionBar

namespace BottomBar {
// Bug report · tutorials · changelog.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace BottomBar

} // namespace SpeedrunLauncher::Main
