#pragma once
#include <optional>
#include <string>
#include <vector>

namespace SpeedrunLauncher::Models {

// A historic build that can be downloaded with SteamCMD ("download_depot app depot manifest").
struct ChapterPreset {
    std::wstring name;
    int appId = 0;
    int depotId = 0;
    std::wstring manifestId;      // ulong kept as text
    std::wstring downloadSize;

    std::wstring Command() const;
};

struct Chapter {
    int number = 0;
    std::wstring title;
    std::wstring subTitle;
    std::wstring description;
    std::wstring steamFolderName;
    // If set, the chapter lives at steamapps\common\Poppy Playtime\{subFolderName}
    std::optional<std::wstring> subFolderName;
    int steamAppId = 0;
    std::optional<std::wstring> gameExePath;     // detected Steam install
    std::optional<std::wstring> detectedVersion;
    bool isAvailable = true;
    std::vector<ChapterPreset> presets;

    bool IsInstalled() const;

    static std::vector<Chapter> All();
};

// A user-added installation (a copy of the game in some folder).
struct Installation {
    std::wstring name;
    std::wstring exePath;
    std::optional<std::wstring> iconPath;
};

} // namespace SpeedrunLauncher::Models
