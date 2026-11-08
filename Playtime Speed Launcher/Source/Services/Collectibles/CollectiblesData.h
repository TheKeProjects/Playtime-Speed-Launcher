#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// 100%: every collectible of Chapters 1 to 5, with the key(s) the game stores it under in the
// inventory. Same list as Assets\Collectibles\overlay.html (keep both in sync):
//   Chapter 1: "vhs:<color>", made up by the live reader (the game saves no tapes)
//   Chapter 2: "vhs:<Name>" / "trophy:<Name>" / "doc:<Name>", made up by the live reader
//   Chapter 3: the item's Name in ItemDataTable (a key of the inventory map), "\0<name>\0"
//   Chapter 4: a row of DT_Items, stored as a length-prefixed name ("\0<row>\0")
//   Chapter 5: an item definition asset ("/<ID_...>.<ID_...>_C")
// An item with several keys counts when any of them is there (alternative versions of the same pickup).
namespace SpeedrunLauncher::Services::Collectibles::Data {

struct Item {
    std::vector<const char*> keys;
    const wchar_t* name;
    const wchar_t* area;   // may be empty
    const wchar_t* hint;   // where it is (Spanish), may be empty
};

struct Category {
    const char* id;
    const char* locKey;    // name in Translations\*.txt
    uint32_t color;        // 0xRRGGBB, same as the overlay
    std::vector<Item> items;
};

struct Chapter {
    int number;
    const wchar_t* title;
    std::vector<Category> cats;
    int Total() const;
};

constexpr int kChapters[] = { 1, 2, 3, 4, 5 };
const Chapter& Get(int number);   // 1 to 5 (anything else: 5)

// Which items are in an inventory / save (the text the server hands the overlay): per category, per item
std::vector<std::vector<bool>> Collected(const Chapter& chapter, std::string_view text);

// Tapes put in their VHS player: the live reader adds "\0played:<key>\0" for each one (Chapters 1 to 4;
// Chapter 5 keeps its tapes in the inventory and nothing tells them apart). total is 0 when the chapter
// can't tell them.
struct PlayedCount { int played = 0, total = 0; };
bool TracksPlayed(const Chapter& chapter);
PlayedCount Played(const Chapter& chapter, std::string_view text);

} // namespace SpeedrunLauncher::Services::Collectibles::Data
