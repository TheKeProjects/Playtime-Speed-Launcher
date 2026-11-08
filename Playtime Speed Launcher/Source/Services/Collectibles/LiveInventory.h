#pragma once
#include <memory>
#include <string>

// Reads the inventory of the running game straight from its memory (read-only, like a speedrun
// autosplitter), so a pickup shows up at once instead of at the next checkpoint save.
//
// Chapters 4 and 5 keep each inventory item as a UPoppyInventoryItemInstance owned by the player
// character. Chapter 4's instance holds the DT_Items row name (an FName); Chapter 5's points to the
// item definition class (ID_..._C). Chapter 3 (UE 5.1) keeps a TMap<FName, int32> in its
// UPlaytimeInventoryComponent: the item's Name -> how many. The objects are found through Unreal's
// global object array and name pool, located by their shape in the exe's .data section (no
// version-specific signatures).
namespace SpeedrunLauncher::Services::Collectibles::Live {

struct Inventory {
    int chapter = 0;
    // One entry per name found in the items, in the same formats the save files use so the overlay
    // matches both the same way: "\0<FName>\0" and "/<ObjectName without _C>."
    std::string text;
    size_t items = 0;
    // The game's process and how many new games the reader has seen start in it since it attached:
    // the launcher's memory of the run (CollectiblesMemory) starts over when this goes up
    uint32_t pid = 0;
    int newGames = 0;
};

// The live inventory of the chapter's game, or null when it isn't running or no player is loaded
// (main menu, loading screen) or its memory couldn't be read. Thread-safe; cached for a moment.
std::shared_ptr<const Inventory> Read(int chapter);

// Forgets what the reader keeps of the chapter's session (the tapes and collectibles seen, the tapes
// played): Settings → 100% → Reset.
void Forget(int chapter);

} // namespace SpeedrunLauncher::Services::Collectibles::Live
