#pragma once
#include <string>
#include <vector>

// The game's save folders (%LOCALAPPDATA%\<project>\Saved\SaveGames) and the bundled checkpoint
// saves the save-card buttons copy into them.
namespace SpeedrunLauncher::Services::SaveGames {

struct Checkpoint {
    std::wstring name;      // folder name, e.g. "[3] Mini Huggies"
    std::wstring source;    // bundled .sav
    std::wstring target;    // where the game reads it
};

// Chapters 1-3: one bundled .sav per checkpoint folder ("[n] name", unnumbered ones first).
std::vector<Checkpoint> Checkpoints(int chapter);
bool LoadCheckpoint(const Checkpoint& checkpoint);

// Chapters 4-5: wipe the save slots / load the bundled "start" saves.
void DeleteSaves(int chapter);
void LoadStartSaves(int chapter);

// Chapter 5 NG+ saves (toggle; a marker file remembers that they are loaded).
bool HasNgPlus();
bool NgPlusLoaded();
void ToggleNgPlus();

} // namespace SpeedrunLauncher::Services::SaveGames
