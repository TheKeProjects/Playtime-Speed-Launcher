#include "Models/MultiplayerGame.h"
#include "Core/Files.h"

namespace SpeedrunLauncher::Models {

bool MultiplayerGame::IsInstalled() const { return gameExePath && !gameExePath->empty() && Files::Exists(*gameExePath); }

std::vector<MultiplayerGame> MultiplayerGame::All() {
    std::vector<MultiplayerGame> all = {
        { "pp", L"Multiplayers\\PP.jpg",
          L"Poppy Playtime - Multiplayer\\Playtime_Multiplayer\\Binaries\\Win64\\Playtime_Multiplayer-Win64-Shipping.exe", std::nullopt, true, 0, 101 },
        { "efp", L"Multiplayers\\EFP.jpg",
          L"Escape From Playtime\\EscapeFromPlaytime\\Binaries\\Win64\\EscapeFromPlaytime-Win64-Shipping.exe", std::nullopt, true, 0, 102 },
    };
    // Escape From Playtime: old builds in the STORE (app 2087540, depot 2087542)
    all[1].presets = {
        { L"Update 3", 2087540, 2087542, L"2931099119965862367", L"" },
    };
    return all;
}

} // namespace SpeedrunLauncher::Models
