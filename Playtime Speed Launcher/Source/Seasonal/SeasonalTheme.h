#pragma once
#include <string>
#include "Graphics/Color.h"

namespace SpeedrunLauncher::Seasonal {

enum class Season { Classic, Halloween, Summer, Christmas };

// Colors of the main screen for one season.
struct Palette {
    Color background;      // window background (under the scenery)
    Color bar;             // top / bottom bars
    Color barBorder;
    Color accent;          // selected card border, card buttons, version label
    Color playBackground;  // PLAY button
    Color playForeground;
    Color title;           // selected chapter title
    Color muted;           // status / description text
};

// The season follows the icon theme setting; themes without visuals use the classic look.
Season Active();
bool IsSeasonal();
std::wstring Name(Season season);   // "classic" | "halloween" | "summer" | "christmas"
void Refresh();                     // re-read after the icon theme changes
unsigned Revision();                // increments when the season changes
const Palette& Current();

// Clock of the seasonal animations (scenery, card decorations, particles). While `paused` (a game
// is running) it slows down smoothly to a stop over ~1.6 s, and speeds back up the same way.
double AnimationTime(double realTime, bool paused);
double LastAnimationTime();   // value returned by the last AnimationTime call
bool AnimationStopped();      // fully stopped: nothing seasonal moves, no need to redraw for it

// Re-tint of the classic teal/blue UI into the season's hues (identity for classic and for colors
// outside the classic palette). Lightness, saturation and alpha are kept.
Color Map(Color c);
// Graphics color filter (applied to settings / overlays / dialogs of the main window).
Color Filter(Color c);

} // namespace SpeedrunLauncher::Seasonal
