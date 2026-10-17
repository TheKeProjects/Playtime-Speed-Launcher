#pragma once
#include <string>
#include "UI/Layout.h"
#include "UI/Ui.h"
#include "UI/Widgets/TextBox.h"

// Building blocks shared by the report forms (bug report, skip report): labelled fields with an
// animated focus glow, an animated status banner and a send button with sending / sent states.
namespace SpeedrunLauncher::UI::Form {

// "ICON  LABEL ✱" above a field. `required` adds the accent star.
void Label(Ui& ui, Column& col, wchar_t glyph, const std::wstring& text, Color accent, bool required = false, float marginBottom = 6.f);

// Text box whose border lights up and glows (eased) while focused; shows "12 / 80" under the
// right edge when the style has a maxLength. Returns true when the text changed.
bool Field(Ui& ui, Id id, const Rect& r, std::wstring& text, const TextBoxStyle& style, Color accent);

enum class Status { Info, Success, Warning, Error };
// Coloured banner (icon + wrapped text) that slides / fades in whenever its text changes.
// Consumes its height from `col`; draws nothing for empty text.
void StatusBanner(Ui& ui, Id id, Column& col, const std::wstring& text, Status kind);
float StatusBannerHeight(Ui& ui, const std::wstring& text, float width);
void StatusBannerAt(Ui& ui, Id id, const Rect& r, const std::wstring& text, Status kind);

enum class SendState { Idle, Sending, Sent };
// Primary button with a send icon; a spinner while sending, a green check once sent.
bool SendButton(Ui& ui, Id id, const Rect& r, const std::wstring& label, Color background, Color foreground, SendState state);

// Eight dots chasing each other around a circle.
void Spinner(Ui& ui, float cx, float cy, float radius, Color color);

} // namespace SpeedrunLauncher::UI::Form
