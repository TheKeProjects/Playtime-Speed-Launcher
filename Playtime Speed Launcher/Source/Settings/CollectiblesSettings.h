#pragma once
#include <string>

namespace SpeedrunLauncher::Settings {

// Settings → 100% (collectibles_settings.json): what the collectibles overlay shows, used by both the
// OBS link and the overlay over the game. Query() turns them into the overlay page's link parameters
// (Assets\Collectibles\overlay.html reads them).
struct Collectibles {
    int chapter = 0;              // 0 automatic (the one open / saved last), 1 to 5
    bool counters = true;         // counters per type
    bool list = false;            // list of collectibles
    bool missingOnly = true;      // list: only what's missing
    bool toasts = true;           // notifications at each pickup
    bool animations = true;
    bool noBackground = false;    // panels without background
    bool right = false;           // against the right edge (OBS source / screen)
    bool english = false;         // overlay language
    double scale = 1.0;
    int listColumns = 1;
    bool inGame = false;          // also over the game, while a chapter is open
    int inGameOffset = 0;         // over the game: DIPs below the top of the screen (dragged in move mode)
    unsigned resetVk = 0x08;      // key that puts every chapter back to 0 while a game is in front (VK_BACK, 0: none)

    std::string Query() const;    // "view=panel&show=cats&..."
    void Save() const;
    static Collectibles& Current();
};

} // namespace SpeedrunLauncher::Settings
