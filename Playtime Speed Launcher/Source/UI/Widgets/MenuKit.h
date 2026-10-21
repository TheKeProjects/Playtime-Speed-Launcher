#pragma once
#include <string>
#include "UI/Style.h"
#include "UI/Ui.h"

// The pieces the redesigned menus share (save card, checkpoints, mod popups, LiveSplit, Add to
// Steam, manual update, tutorials...), so they all look like the Store / Hand Mods / SteamCMD ones.
namespace SpeedrunLauncher::UI::MenuKit {

inline constexpr Color kGreen = Hex(0x00CC88);
inline constexpr Color kRed = Hex(0xCC3333);
inline constexpr Color kAmber = Rgba(255, 176, 32);
inline constexpr Color kDim = Rgba(120, 160, 190, 160);
inline constexpr Color kTitle = Rgba(214, 228, 238);

// Rounded square with a glyph, pulsing glow and a ring breathing out of it
void GlyphBadge(Ui& ui, const Rect& r, wchar_t glyph, Color accent, float glyphSize = 20.f);

// Badge + heading + subtitle in a row of height 52
inline constexpr float kHeroH = 52.f;
void Hero(Ui& ui, const Rect& row, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, Color accent);

// Box with an accent bar on the left, a glyph and wrapped text (warnings / notes). `spin` turns the
// glyph (e.g. the refresh arrow while Steam has to restart).
float NoteHeight(Ui& ui, const std::wstring& text, float width);
void Note(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text, Color accent, bool spin = false);

// Full-width card: glyph badge, title, optional subtitle and a chevron; lifts, lights its border
// and slides the chevron while hovered. Returns true when clicked.
float ActionCardHeight(bool withSubtitle);
bool ActionCard(Ui& ui, Id id, const Rect& r, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, Color accent,
                bool enabled = true);

// Footer buttons: Primary (filled accent, glowing), Secondary (outlined), Danger (red outline)
enum class Kind { Primary, Secondary, Danger };
float FooterButtonWidth(Ui& ui, const std::wstring& text, wchar_t glyph = 0, float minWidth = 110.f);
bool FooterButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, Kind kind, bool enabled = true, wchar_t glyph = 0,
                  Color accent = Palette::Teal);

// Determinate bar with a glow and a light sweeping along the filled part (value eased per id)
void GlowProgress(Ui& ui, Id id, const Rect& r, float value, Color accent = Palette::Teal);

// Small rounded label ("UP TO DATE", "UPDATE AVAILABLE"...); returns its width
float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color accent);

} // namespace SpeedrunLauncher::UI::MenuKit
