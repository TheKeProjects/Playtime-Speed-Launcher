#pragma once
#include "Graphics/Graphics.h"

namespace SpeedrunLauncher::Seasonal::CardArt {

// Decorations on the top edge of a chapter card for the active season (cobwebs, pumpkins,
// seagulls, snow caps...). `variant` keeps each card's decorations stable.
void Draw(Graphics& g, const Rect& card, int variant, float opacity);

} // namespace SpeedrunLauncher::Seasonal::CardArt
