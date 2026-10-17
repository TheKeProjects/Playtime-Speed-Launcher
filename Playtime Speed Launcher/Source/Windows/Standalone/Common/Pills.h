#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "UI/Layout.h"
#include "UI/Ui.h"

// Rounded filter / tag pills in a WrapPanel (Padding 7,3, Margin 0,0,4,4, CornerRadius 10).
namespace SpeedrunLauncher::Windows::Pills {

// Lays out `labels` from col.x, wrapping at col.w. Returns the clicked index (or -1) and advances
// `col` past the panel. `extraWidth` reserves room for a trailing element after the last pill and
// returns where it goes through `trailing`.
int Wrap(UI::Ui& ui, UI::Column& col, std::string_view idBase, const std::vector<std::wstring>& labels, const std::function<bool(size_t)>& active,
         float extraWidth = 0.f, Rect* trailing = nullptr);

float Height(UI::Ui& ui);

} // namespace SpeedrunLauncher::Windows::Pills
