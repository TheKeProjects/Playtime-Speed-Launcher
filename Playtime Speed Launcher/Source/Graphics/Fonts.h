#pragma once
#include <dwrite_3.h>
#include <string>
#include <wrl/client.h>

namespace SpeedrunLauncher {

// The font families the old XAML used.
enum class Font {
    Mono,     // "Cascadia Code, Consolas, Courier New" — almost every label
    Icons,    // "Segoe MDL2 Assets" glyphs (&#xE713; etc.)
    Poppy,    // bundled Assets/Fonts/Poppy Playtime.ttf
    Ui,       // "Segoe UI" (WPF default for unstyled text)
    Consolas, // explicit "Consolas" (arrow buttons)
    // The fonts a launcher account can pick for its name (Fonts::ProfileFont(i)); kept last
    ProfileFirst,
};

namespace Fonts {

struct Family {
    std::wstring name;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;  // null = system collection
};

// Picks the first installed family of each fallback list; loads the bundled font file if present.
void Initialize(const std::wstring& poppyFontPath);
const Family& Get(Font font);

// Account name fonts (Services/Account/Profile.cpp lists them in the same order)
inline constexpr int ProfileFontCount = 12;
Font ProfileFont(int index);

} // namespace Fonts
} // namespace SpeedrunLauncher
