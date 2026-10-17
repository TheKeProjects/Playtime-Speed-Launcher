#pragma once
#include <string>
#include "UI/Layout.h"
#include "UI/Widgets/TextBox.h"
#include "UI/Ui.h"

// Building blocks for dialog contents (the TextBlocks / Borders / TextBoxes / CheckBoxes the old
// code assembled into StackPanels for WpfDialog). Each one consumes a row of a UI::Column.
namespace SpeedrunLauncher::Dialogs::Parts {

// Wrapped TextBlock.
void Label(UI::Ui& ui, UI::Column& col, const std::wstring& text, float size, Color color, bool bold = false, float marginBottom = 0.f);
// Rounded box with text inside (the "command" chips, folder paths...).
void Box(UI::Ui& ui, UI::Column& col, const std::wstring& text, float size, Color background, Color border, Color foreground,
         Edges padding = { 10.f, 6.f }, float marginBottom = 0.f, bool bold = false);
// WPF CheckBox: 13 px box + wrapped caption. Returns true when toggled.
bool CheckBox(UI::Ui& ui, UI::Id id, UI::Column& col, const std::wstring& text, bool& checked, float size = 11.f,
              Color textColor = Rgba(160, 200, 230, 200));
// The dark text inputs used in every dialog.
UI::TextBoxStyle InputStyle(bool password = false);
bool Input(UI::Ui& ui, UI::Id id, UI::Column& col, std::wstring& value, bool password = false, float marginBottom = 0.f);
// MakeSmallButton: Height 28, MinWidth 80, Padding 8,0, #081E37 background.
bool SmallButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, Color foreground, bool enabled = true);
float SmallButtonWidth(UI::Ui& ui, const std::wstring& text, float minWidth = 80.f);

} // namespace SpeedrunLauncher::Dialogs::Parts
