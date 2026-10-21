#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <vector>
#include "UI/Ui.h"
#include "UI/Widgets/Form.h"

// The look shared by the bug report and the skip report (same layout as the Inazuma editor's
// reports tab): an animated banner, the form in a card on the left, and on the right a checklist
// that ticks itself as the report gets ready, a tip and a big send button.
namespace SpeedrunLauncher::Main::ReportKit {

inline constexpr float kRadius = 12.f, kGap = 14.f, kBannerH = 92.f;

// Gradient strip in `accent` with a drifting light, a badge whose glyph wobbles, the title, a
// subtitle and a pill on the right
void Banner(UI::Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, const std::wstring& pill,
            Color accent);

// Rounded card; returns its content area (inset by `pad`)
Rect Card(UI::Ui& ui, const Rect& r, Color accent, float pad = 18.f);
// Small caps caption with a glyph; returns its height
float Caption(UI::Ui& ui, float x, float y, float w, wchar_t glyph, const std::wstring& text, Color accent);

// "Title *" on the left, "12 / 100" on the right (amber when full)
void FieldLabel(UI::Ui& ui, const Rect& r, const std::wstring& text, size_t count, size_t limit, Color accent, bool required);

// Drop-zone look for an optional screenshot: dashes drifting round the outline while hovered, an
// image glyph bobbing; once picked, its preview, name and a remove button. Click to pick.
void Screenshot(UI::Ui& ui, UI::Id id, const Rect& r, std::optional<std::wstring>& image, Color accent, HWND owner, const std::wstring& title,
                const std::wstring& formats, const std::wstring& attached);

struct Check {
    std::wstring title, hint;
    bool done = false, optional = false;
};
// Caption + a progress ring (required steps done / total), then the steps: numbered circles that
// turn into green checks with a pulse, joined by a line that fills in; a tip box at the bottom
// when there's room
void Checklist(UI::Ui& ui, const Rect& r, const std::vector<Check>& checks, Color accent, double start, const std::wstring& caption,
               const std::wstring& readyText, const std::wstring& pendingText, const std::wstring& optionalText, const std::wstring& tip);

// The send button: glows while the report is ready, the paper plane takes off on hover, a spinner
// while sending and a green burst + check once sent. Returns true when clicked.
bool SendButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& label, Color accent, bool ready, UI::Form::SendState state);

// Status message as a pill sliding in from the left (empty text draws nothing)
void StatusPill(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, UI::Form::Status kind);

} // namespace SpeedrunLauncher::Main::ReportKit
