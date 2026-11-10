#pragma once
#include <map>
#include <string>
#include <vector>
#include "Core/Json.h"

// A launcher account's public profile (table launcher_profiles): the name and how it's drawn
// (font + effect + colours, like Discord's display name styles), the round picture and the banner
// (both may be GIFs) and the linked speedrun.com and Discord accounts.
namespace SpeedrunLauncher::Services::Account {

struct NameFont {
    const char* id;        // stored in the profile
    int fontIndex;         // Fonts::ProfileFont(index)
};
struct NameEffect {
    const char* id;
    const char* locKey;    // its name in the effect picker
    bool twoColors;        // uses the second colour
    bool animated;
};

const std::vector<NameFont>& NameFonts();
const std::vector<NameEffect>& NameEffects();
// Cosmetics (drawn by Windows\Main\Common\Cosmetics.cpp): a decoration around the profile picture,
// a frame around the profile card and a nameplate behind the name in lists. "none" = nothing.
struct Cosmetic {
    const char* id;        // stored in the profile
    const char* locKey;    // its name in the picker
    bool animated;
};
const std::vector<Cosmetic>& AvatarDecorations();
const std::vector<Cosmetic>& ProfileFrames();
const std::vector<Cosmetic>& Nameplates();

// Colour choices for the name and the banner (0xRRGGBB)
const std::vector<unsigned>& NameColors();
const std::vector<unsigned>& BannerColors();
const std::vector<unsigned>& CardColors();
// The card colour's default (stored as ''): the classic dark navy card, whatever the theme
inline constexpr unsigned kAutoColor = 0xFF000000u;
inline constexpr unsigned kDefaultCardColor = 0x0B1620;

struct Profile {
    std::wstring userId;
    std::wstring displayName;
    std::wstring avatar;        // file name in the bucket folder of the user, empty = none
    std::wstring banner;
    unsigned bannerColor = 0x1A3A55;
    std::string font = "default";
    std::string effect = "solid";
    unsigned color1 = 0xD2DCE6;
    unsigned color2 = 0x00CCAA;
    unsigned cardColor = kAutoColor;   // the card's body (kAutoColor = kDefaultCardColor)
    std::string avatarDeco = "none";   // Cosmetic ids (AvatarDecorations / ProfileFrames / Nameplates)
    std::string frame = "none";
    std::string nameplate = "none";
    // Linked speedrun.com account (only the server writes these)
    std::wstring speedrunId, speedrunName, speedrunUrl, speedrunAvatar;
    // Linked Discord account (only the server writes these; the avatar is a full URL)
    std::wstring discordId, discordName, discordAvatar;
    // Hours per chapter on the account (community / friends / search lists)
    std::map<int, double> playtime;

    bool SpeedrunLinked() const { return !speedrunId.empty(); }
    bool DiscordLinked() const { return !discordId.empty(); }
    int FontIndex() const;
    const NameEffect& Effect() const;
    // Public URLs of the pictures (empty when there's none)
    std::wstring AvatarUrl() const;
    std::wstring BannerUrl() const;
    // The name to show: the display name, else the speedrun.com name, else `fallback`
    std::wstring ShownName(const std::wstring& fallback) const;

    // Row of launcher_profiles / launcher_community
    static Profile FromJson(const Json::Value& row);
    // The columns the user may write
    Json::Value EditableJson() const;
    // Everything, for the local cache
    Json::Value CacheJson() const;
    bool SameStyle(const Profile& other) const;
};

std::wstring PictureUrl(const std::wstring& userId, const std::wstring& file);
std::wstring ColorText(unsigned rgb);                   // "#1A3A55" ("" for kAutoColor)
unsigned ParseColor(const std::string& text, unsigned fallback);
// "a*******@gmail.com" style: the part before the @ hidden
std::wstring MaskEmail(const std::wstring& email);

} // namespace SpeedrunLauncher::Services::Account
