#pragma once
#include <string>
#include <vector>
#include "Services/Account/DuelsService.h"
#include "UI/Ui.h"
#include "UI/Widgets/MenuKit.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"

// Look shared by the duels menu's pages (DuelsOverlay.cpp defines them).
namespace SpeedrunLauncher::Main::DuelsKit {

using namespace AccountKit;
using Services::Account::DuelModeLabel;
using Services::Account::FormatDuelTime;

inline constexpr Color kDuel = Hex(0xFF5A3C);      // the duels' accent (versus red-orange)
inline constexpr Color kGold = Hex(0xFFB020);
inline constexpr Color kOnline = Hex(0x3DDC84);
inline constexpr Color kDanger = Hex(0xCC4422);
inline constexpr wchar_t kDuelGlyph = 0xE916;      // stopwatch (buttons / chips)

// The duels' icon: a stopwatch rushing forward, with speed lines trailing behind it. `motion`
// (0..1, e.g. hover) stretches the lines and makes them flicker.
void SpeedWatchIcon(UI::Ui& ui, const Rect& r, float size, Color color, float motion = 0.f);

std::wstring NameOf(const Services::Account::Profile& p);
Color RankColor(int mmr);
// The rank's icon (the bot's emoji) fitted in `r`, in its own colours
void RankIcon(UI::Ui& ui, const Rect& r, int mmr, float opacity = 1.f);
void RankIconOf(UI::Ui& ui, const Rect& r, const Services::Account::DuelRank& rank, float opacity = 1.f);
// "DogDay · 163 MMR" pill with the rank's colour; returns its width
float RankChip(UI::Ui& ui, float x, float cy, int mmr, bool withMmr = true);
// Small rounded label; returns its width
float Chip(UI::Ui& ui, float x, float cy, const std::wstring& text, Color color, wchar_t glyph = 0);
// A row of pill buttons; returns the clicked index or -1
int Pills(UI::Ui& ui, const Rect& r, const std::vector<std::wstring>& labels, int selected, std::string_view idBase, Color accent = kDuel,
          float minWidth = 0.f);
// Big empty state: glyph + wrapped message
void EmptyState(UI::Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text);
bool IconButton(UI::Ui& ui, UI::Id id, const Rect& r, wchar_t glyph, UI::MenuKit::Kind kind, bool enabled, const std::wstring& tooltip, Color accent = kDuel);
// "Random chapter & category" / "Chapter 3 · Any% NMS" / "6 bans"
std::wstring DuelTypeText(const Services::Account::Duel& duel);
std::wstring CategoryText(const std::string& categoryId);   // "Ch3 · Any% NMS"
// Medal colour for a placement (gold / silver / bronze / plain)
Color PlaceColor(int placement);

} // namespace SpeedrunLauncher::Main::DuelsKit
