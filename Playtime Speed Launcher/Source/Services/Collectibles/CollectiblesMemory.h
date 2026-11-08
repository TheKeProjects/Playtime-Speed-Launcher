#pragma once
#include <string>

// 100%: the launcher's own record of what was picked up in each chapter. Every read of the running
// game (LiveInventory) adds what's new to it and nothing ever takes it away: not a reloaded checkpoint,
// not going back (Chapter 4 takes the tape away), not the Chapter 5 loading screen glitch, not closing
// the game, not loading a save. It starts over by hand (Settings → 100% → Reset) and, in Chapters 4 and
// 5, when the game writes a save at the very start of the chapter (a new run: loading a save writes
// none) (Inventory::newGames). Chapters 1 to 3 save nothing that tells a new game from a loaded save.
// Saved in collectibles_memory.json, so it survives closing the launcher.
// The save files aren't read for the progress any more.
namespace SpeedrunLauncher::Services::Collectibles::Memory {

struct Result {
    std::string text;    // what's remembered, in the inventory text format Data::Collected / the overlay read
    bool live = false;   // the game is open and was just read
    bool any = false;    // something is remembered
};

// Reads the chapter's game if it's open, adds what's new to the memory and hands out the memory.
// Thread-safe.
Result Read(int chapter);

// Forgets the chapter's run.
void Reset(int chapter);
// Forgets every chapter's run (the reset key, Settings → 100%).
void ResetAll();

} // namespace SpeedrunLauncher::Services::Collectibles::Memory
