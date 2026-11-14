#pragma once
#include <cstdint>
#include <string>

namespace SpeedrunLauncher::Settings {

// Escape From Playtime "Custom Name" mod (old versions): one name and style for every version it's
// installed in. custom_name.json; each install gets a copy as the mod's config.ini.
struct CustomName {
    std::wstring name;
    uint32_t color = 0xFFFFFF;     // 0xRRGGBB
    uint32_t color2 = 0x33CCFF;    // second colour (gradient / glitch)
    std::wstring animation = L"none";
    float speed = 1.f;             // 0.25 .. 3
    float size = 1.f;              // 0.8 .. 1.6
    bool shadow = true;
    bool outline = false;
    bool bold = false;             // old setting, read as typeface "Bold"
    std::wstring typeface = L"Regular";   // Regular (the game's own) / Bold
    bool italic = false;           // the font's Italic / BoldItalic typeface
    std::wstring letterCase = L"typed";   // typed (as written) / upper / lower: forced over the game's UPPERCASE
    bool spacing = false;          // wide letter spacing
    bool style = true;             // false: only the name changes, the game's own look stays

    static constexpr size_t kMaxName = 20;   // the engine cuts login names at 20 characters

    void Save() const;
    // The config.ini the Lua mod reads
    std::wstring ToGameConfig() const;
    static CustomName& Current();
};

} // namespace SpeedrunLauncher::Settings
