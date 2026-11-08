#pragma once
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

namespace TopBar {
// "Approved by SPEEDRUN.COM", online users counter, minimize button (windowed mode), version.
// Returns the x where the right-hand items start (the Steam user badge goes left of it).
float Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace TopBar

namespace NavRail {
// Left column: Hand Mods · leaderboard · updates & changelog (+ update badge) · tutorials · bug
// report · add to Steam, and theme · settings · quit at the bottom. Each one an icon with its label.
void Draw(UI::Ui& ui, MainWindow& main, const Rect& area);
} // namespace NavRail

} // namespace SpeedrunLauncher::Main
