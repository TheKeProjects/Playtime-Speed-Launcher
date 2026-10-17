#pragma once
#include "Graphics/Graphics.h"

// Vector icons that were DrawingImage resources in App.xaml.
namespace SpeedrunLauncher::UI::Icons {

void LiveSplit(Graphics& g, const Rect& r);     // hexagon with the three coloured circles
void LoadManip(Graphics& g, const Rect& r, Color c = Hex(0x00CCAA));  // folder + gear

} // namespace SpeedrunLauncher::UI::Icons
