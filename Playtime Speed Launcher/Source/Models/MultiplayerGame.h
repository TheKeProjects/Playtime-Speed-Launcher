#pragma once
#include <optional>
#include <string>
#include <vector>

namespace SpeedrunLauncher::Models {

// A Poppy Playtime multiplayer game (the Multiplayers tab of the main screen). Texts come from
// the translations as "mp_<key>_title" / "mp_<key>_desc".
struct MultiplayerGame {
    std::string key;
    std::wstring banner;                       // under Assets\Banners
    std::wstring steamExe;                     // relative to steamapps\common
    std::optional<std::wstring> gameExePath;   // detected Steam install
    bool isAvailable = true;                   // false = not out yet (locked card)
    int steamAppId = 0;                        // from the library's appmanifest (0 = not found)
    int playtimeId = 0;                        // its key in the playtime store (past the chapter numbers)

    bool IsInstalled() const;

    static std::vector<MultiplayerGame> All();
};

} // namespace SpeedrunLauncher::Models
