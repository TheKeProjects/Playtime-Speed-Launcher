#pragma once
#include <string>
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

// Wrapped changelog text where every "@name" becomes a Discord-coloured chip (clickable when the
// name is a known contributor: opens their Discord profile). Returns the height used.
// With draw = false it only measures (nothing is drawn or made clickable).
float MentionText(UI::Ui& ui, UI::Id id, float x, float y, float width, const std::wstring& text, float fontSize, Color color,
                  bool draw = true);

} // namespace SpeedrunLauncher::Main
