#pragma once
#include <string>
#include "UI/Style.h"
#include "UI/Ui.h"
#include "UI/Widgets/TextBox.h"

// Look shared by the account menu's pages.
namespace SpeedrunLauncher::Main::AccountKit {

inline constexpr Color kAccent = Hex(0x00CCAA);
inline constexpr Color kBlurple = Rgba(0x58, 0x65, 0xF2);
inline constexpr Color kSpeedrun = Hex(0xFFB020);   // speedrun.com's trophy gold
// Colours from the launcher's classic palette: every theme re-tints them like the other menus
inline constexpr Color kCard = Hex(0x0A1420);
inline constexpr Color kCardBorder = Hex(0x153048);
inline constexpr Color kRow = Hex(0x0A1825);        // list rows / tiles
inline constexpr Color kRowHover = Hex(0x0F2438);
inline constexpr Color kHoverBorder = Hex(0x2A5A7A);
inline constexpr Color kText = Hex(0xD2DCE6);
inline constexpr Color kSub = Hex(0x78A0BE);

inline UI::TextBoxStyle Field(bool password = false, size_t maxLength = 0) {
    UI::TextBoxStyle s;
    s.text = UI::Mono(11.5f);
    s.foreground = Hex(0xCCEEFF);
    s.background = Hex(0x060F18);
    s.border = kCardBorder;
    s.focusBorder = kAccent;
    s.caret = kAccent;
    s.selection = Rgba(0, 204, 170, 70);
    s.padding = { 12.f, 9.f };
    s.radius = 8.f;
    s.password = password;
    s.maxLength = maxLength;
    return s;
}

// Small caps caption ("NAME", "PICK A FONT")
inline void Caption(UI::Ui& ui, float x, float y, float w, const std::wstring& text, Color color = kSub) {
    ui.g.Text(text, { x, y, w, 16.f }, color, UI::Mono(9.5f, true));
}

// Rounded card background
inline void Card(UI::Ui& ui, const Rect& r, Color accent = kCardBorder) {
    ui.g.Border(r, 12.f, kCard, accent, 1.f);
}

} // namespace SpeedrunLauncher::Main::AccountKit
