#pragma once
#include <string>
#include "Services/Account/BadgesService.h"
#include "Services/Account/Profile.h"
#include "UI/Ui.h"

// How a launcher account looks wherever it shows up (top bar, account menu, community ranking):
// the name in its font and effect, the round picture and the banner (GIFs move).
namespace SpeedrunLauncher::Main::ProfileDraw {

using Services::Account::Profile;

// Pictures are given as sources (a local file while editing, else the profile's URL; empty = none)
inline constexpr unsigned kAvatarSide = 192;
inline constexpr unsigned kBannerSide = 760;

TextStyle NameStyle(const Profile& profile, float size);
float NameWidth(UI::Ui& ui, const std::wstring& text, const Profile& profile, float size);
// The name in the profile's font + effect, vertically centred in `r`
void Name(UI::Ui& ui, const std::wstring& text, const Rect& r, const Profile& profile, float size, HAlign align = HAlign::Left);
// The same text drawn with one effect (the effect picker's previews)
void NameWithEffect(UI::Ui& ui, const std::wstring& text, const Rect& r, const Profile& profile, const char* effect, float size,
                    HAlign align = HAlign::Center);

// Round picture; with no picture, a circle in the name's colour with the first letter.
// `animate` false keeps a GIF on its first frame (the main screen while a game is running).
// `decoration` false: the caller draws AvatarDecoration itself, after what goes over the picture
// (presence dot, the row's texts) so the decoration stays on top
void Avatar(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile, const std::wstring& name, float ring = 0.f,
            Color ringColor = Colors::Transparent, bool animate = true, bool decoration = true);
// The banner filling `r` (picture, or the banner colour as a soft gradient)
void Banner(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile, float radius);
// For rects much wider than a banner (the full profile's header): the whole picture, uncropped
void WideBanner(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile);

// The Discord-like profile card: banner, the picture overlapping it, the name and two lines under it
struct CardInfo {
    std::wstring name;
    std::wstring avatarSource, bannerSource;
    std::wstring line1, line2;     // e.g. the speedrun.com link and the hours
    Color line1Color = Hex(0x8AAABB);
    const Services::Account::Badge* badge = nullptr;   // the badge next to the name (BadgesService::Shown)
};
float CardHeight(float width);
void Card(UI::Ui& ui, const Rect& r, const Profile& profile, const CardInfo& info);
// The card body's colour (the theme's panel colour for kAutoColor)
Color CardColor(const Profile& profile);

// ── Cosmetics (Cosmetics.cpp) ──
// The profile's decoration around a picture drawn in `avatar` (Avatar draws it by itself)
void AvatarDecoration(UI::Ui& ui, const Rect& avatar, const Profile& profile, bool animate = true);
// The profile's frame around a card (Card draws it by itself); `radius` = the card's corners
void ProfileFrame(UI::Ui& ui, const Rect& card, float radius, const Profile& profile);
// The profile's nameplate behind a list row (call right after drawing the row's background)
void Nameplate(UI::Ui& ui, const Rect& row, float radius, const Profile& profile);
bool HasNameplate(const Profile& profile);
// A row's text (times, places, MMR...): on a nameplate (`onPlate`) it gets a dark halo so it reads
// on any plate; else plain text
void RowText(UI::Ui& ui, const std::wstring& text, const Rect& r, Color color, const TextStyle& style, bool onPlate);
// A soft dark veil behind something drawn over a nameplate (text, chips) so it stands out
void PlateScrim(UI::Ui& ui, const Rect& bounds);

// Hours per chapter / multiplayer game: a label, a bar against the most played one and the time.
// Returns the height used (draws nothing but "—" when there are no hours).
float HoursListHeight(const Profile& profile);
float HoursList(UI::Ui& ui, const Rect& r, const Profile& profile, Color accent);
double TotalSeconds(const Profile& profile);

} // namespace SpeedrunLauncher::Main::ProfileDraw
