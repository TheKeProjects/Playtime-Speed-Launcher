#include "Models/MultiplayerGame.h"
#include "Core/Files.h"

namespace SpeedrunLauncher::Models {

bool MultiplayerGame::IsInstalled() const { return gameExePath && !gameExePath->empty() && Files::Exists(*gameExePath); }

std::vector<MultiplayerGame> MultiplayerGame::All() {
    return {
        { "pp", L"Multiplayers\\PP.jpg",
          L"Poppy Playtime - Multiplayer\\Playtime_Multiplayer\\Binaries\\Win64\\Playtime_Multiplayer-Win64-Shipping.exe", std::nullopt, true, 0, 101 },
        { "efp", L"Multiplayers\\EFP.jpg",
          L"Escape From Playtime\\EscapeFromPlaytime\\Binaries\\Win64\\EscapeFromPlaytime-Win64-Shipping.exe", std::nullopt, true, 0, 102 },
    };
}

} // namespace SpeedrunLauncher::Models
