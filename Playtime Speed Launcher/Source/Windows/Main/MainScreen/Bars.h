#pragma once
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

namespace TopBar {
// "Approved by SPEEDRUN.COM", online users counter, minimize button (windowed mode), version.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace TopBar

namespace ActionBar {
// Hand mods · leaderboard · updates & changelog (+ update badge) · settings | PLAY CHAPTER | quit · add to Steam.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace ActionBar

namespace BottomBar {
// Bug report · tutorials.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace BottomBar

} // namespace SpeedrunLauncher::Main
