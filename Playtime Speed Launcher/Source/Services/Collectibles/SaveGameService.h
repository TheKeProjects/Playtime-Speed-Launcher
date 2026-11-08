#pragma once
#include <cstdint>
#include <memory>
#include <string>

// 100%: finds the save game of the chapter being played and hands it out decompressed, so the
// collectibles overlay can tell which collectibles are in the inventory:
//   Chapter 1  only the checkpoint number: its tapes are read live (LiveInventory)
//   Chapter 2  %LOCALAPPDATA%\Playtime_Prototype4\Saved\SaveGames\Chap2Checkpoint.sav (plain GVAS; trophies)
//   Chapter 3  %LOCALAPPDATA%\Playtime_Chapter3\Saved\SaveGames\Playtime.sav     (plain GVAS, rewritten at each checkpoint)
//   Chapter 4  %LOCALAPPDATA%\ch4_pro\Saved\SaveGames\SaveGame_<date>.sav   (zlib-compressed archive)
//   Chapter 5  %LOCALAPPDATA%\ch5_pro\Saved\SaveGames\PoppySave_<date>.sav  (plain GVAS)
// The game writes a new file on every checkpoint, named after the date, so the newest name is the
// latest progress.
namespace SpeedrunLauncher::Services::Collectibles::SaveGames {

struct Save {
    int chapter = 0;
    std::wstring path;
    int64_t lastWrite = 0;
    uint64_t size = 0;
    std::string data;            // decompressed save
    std::string Stamp() const;   // changes whenever the file does (HTTP ETag)
};

struct ChapterInfo {
    int chapter = 0;
    std::wstring folder;
    bool folderExists = false;
    bool running = false;            // the chapter's exe is open
    size_t saves = 0;
    std::wstring newest;             // file name of the newest save, empty if none
    int64_t newestWrite = 0;
    bool waitingFirstSave = false;   // running, but nothing saved since it was opened
};

constexpr int kChapters[] = { 1, 2, 3, 4, 5 };

ChapterInfo Info(int chapter);

// The chapter to show when the overlay asks for "auto": the one running, else the one saved last
int AutoChapter();

// True while the chapter's game is open and this save was written before it was opened: a new game
// (or a loaded one) whose first checkpoint hasn't been saved yet. The old save isn't this run's progress.
bool FromBeforeLaunch(const Save& save);

// What the save adds to the live inventory while the game is open: Chapter 2's trophies (the game
// writes them at once, and they leave its memory at the next checkpoint reload). Empty for the others,
// or when the save is from before the game was opened.
std::string AlongLive(int chapter);

// Newest save of the chapter, decompressed, or null. Thread-safe; rereads only when the file changes.
std::shared_ptr<const Save> Current(int chapter);

} // namespace SpeedrunLauncher::Services::Collectibles::SaveGames
