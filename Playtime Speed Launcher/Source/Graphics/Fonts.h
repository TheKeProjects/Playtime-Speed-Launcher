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
};

namespace Fonts {

struct Family {
    std::wstring name;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;  // null = system collection
};

// Picks the first installed family of each fallback list; loads the bundled font file if present.
void Initialize(const std::wstring& poppyFontPath);
const Family& Get(Font font);

} // namespace Fonts
} // namespace SpeedrunLauncher
