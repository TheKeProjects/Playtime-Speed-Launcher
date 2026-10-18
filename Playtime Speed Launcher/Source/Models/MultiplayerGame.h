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

    bool IsInstalled() const;

    static std::vector<MultiplayerGame> All();
};

} // namespace SpeedrunLauncher::Models
