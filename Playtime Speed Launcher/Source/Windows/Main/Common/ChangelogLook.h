#pragma once
#include <string>
#include "Graphics/Graphics.h"
#include "UI/Style.h"

// Colour and glyph of each changelog "## " section (version history and the update window).
namespace SpeedrunLauncher::Main::ChangelogLook {

struct SectionLook {
    Color color;
    wchar_t glyph;   // Segoe MDL2 Assets
};

inline SectionLook ForSection(const std::wstring& title) {
    if (title == L"Additions") return { Rgba(46, 204, 140), 0xE710 };          // + (Add)
    if (title == L"Bug Fixes") return { Rgba(255, 152, 72), 0xE90F };          // wrench (Repair)
    if (title == L"Other") return { Rgba(110, 168, 240), 0xE946 };             // (i) (Info)
    if (title == L"Redesigns") return { Rgba(186, 140, 255), 0xE790 };         // palette (Color)
    if (title == L"Seasonal Themes") return { Rgba(255, 196, 64), 0xE706 };    // sun (Brightness)
    if (title == L"Performance") return { Rgba(80, 206, 236), 0xE945 };        // lightning bolt
    return { Hex(0x00CCAA), 0xE73E };                                          // anything else: check mark
}

// "Leaderboards: click a runner…" → length of "Leaderboards:" (0 when the line has no short
// "topic:" prefix). Drawn bold in the section colour over the line.
inline size_t TopicLength(const std::wstring& line) {
    const size_t colon = line.find(L": ");
    if (colon == std::wstring::npos || colon == 0 || colon > 28) return 0;
    if (line.find(L'@') < colon) return 0;
    return colon + 1;
}

// A wrapped change with its "Topic:" prefix in bold and the section colour. The font is
// monospaced, so the prefix is blanked out of the wrapped line and drawn on top at the same advance.
inline void DrawLine(Graphics& g, const std::wstring& text, const Rect& r, float size, Color accent) {
    const TextStyle style = UI::Mono(size).Wrap().Top();
    const size_t topic = TopicLength(text);
    if (!topic) {
        g.Text(text, r, UI::Palette::Label, style);
        return;
    }
    g.Text(std::wstring(topic, L' ') + text.substr(topic), r, UI::Palette::Label, style);
    g.Text(text.substr(0, topic), r, Lerp(accent, Colors::White, 0.25f), UI::Mono(size, true).Top());
}

} // namespace SpeedrunLauncher::Main::ChangelogLook
