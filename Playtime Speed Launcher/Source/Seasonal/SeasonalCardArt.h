#pragma once
#include "Graphics/Graphics.h"

namespace SpeedrunLauncher::Seasonal::CardArt {

// Decorations on the top edge of a chapter card for the active season (cobwebs, pumpkins,
// seagulls, snow caps...). `variant` keeps each card's decorations stable.
void Draw(Graphics& g, const Rect& card, int variant, float opacity);

// The same dressing for the standalone menus over the game (checkpoint picker, skips and glitches):
//   DrawPanelParticles  snow / embers and a bat / bubbles and glints inside the panel (clipped to it)
//   DrawPanelEdge       pieces resting on the panel's top edge (snow cap, Santa hat, pumpkins, crab...)
// time (seconds) drives the animations. Both draw nothing outside a season.
void DrawPanelParticles(Graphics& g, const Rect& panel, float radius, double time, float opacity = 1.f);
void DrawPanelEdge(Graphics& g, const Rect& panel, double time, float opacity = 1.f);

} // namespace SpeedrunLauncher::Seasonal::CardArt
