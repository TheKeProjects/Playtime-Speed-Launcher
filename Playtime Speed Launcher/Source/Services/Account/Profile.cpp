#include "Services/Account/Profile.h"
#include "App/Config.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Services::Account {

const std::vector<NameFont>& NameFonts() {
    // Same order as Fonts::ProfileFont (Graphics/Fonts.cpp)
    static const std::vector<NameFont> fonts = {
        { "default", 0 }, { "poppy", 1 },   { "mono", 2 },   { "impact", 3 }, { "serif", 4 },  { "comic", 5 },
        { "script", 6 },  { "print", 7 },   { "ink", 8 },    { "bahn", 9 },   { "gabriola", 10 }, { "courier", 11 },
    };
    return fonts;
}

const std::vector<NameEffect>& NameEffects() {
    static const std::vector<NameEffect> effects = {
        { "solid", "acc_effect_solid", false, false },     { "gradient", "acc_effect_gradient", true, false },
        { "neon", "acc_effect_neon", false, false },       { "toon", "acc_effect_toon", true, false },
        { "pop", "acc_effect_pop", true, false },          { "gummy", "acc_effect_gummy", true, false },
        { "prism", "acc_effect_prism", false, true },
    };
    return effects;
}

const std::vector<Cosmetic>& AvatarDecorations() {
    static const std::vector<Cosmetic> list = {
        { "none", "cos_none", false },        { "sparkles", "cos_deco_sparkles", true }, { "flames", "cos_deco_flames", true },
        { "neon", "cos_deco_neon", true },    { "grabpack", "cos_deco_grabpack", true }, { "grabpack67", "cos_deco_grabpack67", true }, { "scanner", "cos_deco_scanner", true }, { "stopwatch", "cos_deco_stopwatch", true },
        { "toxic", "cos_deco_toxic", true },  { "hearts", "cos_deco_hearts", true },     { "frost", "cos_deco_frost", true },
        { "crown", "cos_deco_crown", true },  { "halo", "cos_deco_halo", true },
    };
    return list;
}

const std::vector<Cosmetic>& ProfileFrames() {
    static const std::vector<Cosmetic> list = {
        { "none", "cos_none", false },          { "gold", "cos_frame_gold", true },   { "neon", "cos_frame_neon", true },
        { "hazard", "cos_frame_hazard", false }, { "toxic", "cos_frame_toxic", true }, { "frost", "cos_frame_frost", true },
        { "checkered", "cos_frame_checkered", false }, { "prisma", "cos_frame_prisma", true }, { "ember", "cos_frame_ember", true },
    };
    return list;
}

const std::vector<Cosmetic>& Nameplates() {
    static const std::vector<Cosmetic> list = {
        { "none", "cos_none", false },          { "banner", "cos_plate_banner", false }, { "name", "cos_plate_name", false },
        { "sunset", "cos_plate_sunset", false }, { "ocean", "cos_plate_ocean", false },   { "galaxy", "cos_plate_galaxy", true },
        { "toxic", "cos_plate_toxic", true },   { "ember", "cos_plate_ember", true },    { "hazard", "cos_plate_hazard", false },
        { "checkered", "cos_plate_checkered", false }, { "prisma", "cos_plate_prisma", true },
    };
    return list;
}

namespace {
// Any well-formed id is kept, even one this launcher doesn't know (a newer launcher's cosmetic):
// it just isn't drawn here, and saving the profile from here doesn't wipe it
std::string Known(const std::vector<Cosmetic>& list, const std::string& id) {
    (void)list;
    if (id.empty() || id.size() > 24) return "none";
    for (const char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return "none";
    return id;
}
} // namespace

const std::vector<unsigned>& NameColors() {
    static const std::vector<unsigned> colors = {
        0xD2DCE6, 0xFFFFFF, 0x00CCAA, 0x3DDC84, 0x5865F2, 0x3A9BFF, 0xB46CFF, 0xFF6FD8,
        0xFF4D5E, 0xFF8A3D, 0xFFD23F, 0x8B5A2B, 0x1B1B2F, 0x7A8899,
    };
    return colors;
}

const std::vector<unsigned>& BannerColors() {
    static const std::vector<unsigned> colors = {
        0x1A3A55, 0x0B2A24, 0x13294B, 0x2B1748, 0x4A1230, 0x4A1A12, 0x4A3A10, 0x1E1E24,
        0x00CCAA, 0x5865F2, 0xB46CFF, 0xFF4D5E, 0xFF8A3D, 0xFFD23F,
    };
    return colors;
}

const std::vector<unsigned>& CardColors() {
    // Dark and mid tones (the card's texts are light)
    static const std::vector<unsigned> colors = {
        kAutoColor, 0x15181E, 0x1E2230, 0x13294B, 0x0B2A24, 0x1F3A1A, 0x2B1748, 0x3B1F5C,
        0x4A1230, 0x4A1A12, 0x4A3410, 0x3A2E12, 0x0F3A44, 0x24303A,
    };
    return colors;
}

std::wstring ColorText(unsigned rgb) {
    if (rgb == kAutoColor) return L"";
    wchar_t buffer[16];
    swprintf_s(buffer, L"#%06X", rgb & 0xFFFFFFu);
    return buffer;
}

unsigned ParseColor(const std::string& text, unsigned fallback) {
    if (text.size() != 7 || text[0] != '#') return fallback;
    unsigned value = 0;
    for (size_t i = 1; i < 7; ++i) {
        const char c = text[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return fallback;
        value = value * 16 + static_cast<unsigned>(digit);
    }
    return value;
}

std::wstring MaskEmail(const std::wstring& email) {
    const size_t at = email.find(L'@');
    if (at == std::wstring::npos) return L"*******";
    // Always seven stars: the length of the name isn't given away either
    return L"*******" + email.substr(at);
}

std::wstring PictureUrl(const std::wstring& userId, const std::wstring& file) {
    if (userId.empty() || file.empty()) return L"";
    return std::wstring(Config::Supabase::Url) + L"/storage/v1/object/public/" + Config::Supabase::ProfilesBucket + L"/" +
           Text::UrlEncode(userId) + L"/" + Text::UrlEncode(file);
}

int Profile::FontIndex() const {
    for (const auto& f : NameFonts())
        if (font == f.id) return f.fontIndex;
    return 0;
}

const NameEffect& Profile::Effect() const {
    for (const auto& e : NameEffects())
        if (effect == e.id) return e;
    return NameEffects().front();
}

std::wstring Profile::AvatarUrl() const { return PictureUrl(userId, avatar); }
std::wstring Profile::BannerUrl() const { return PictureUrl(userId, banner); }

std::wstring Profile::ShownName(const std::wstring& fallback) const {
    const std::wstring name = Text::Trim(displayName);
    if (!name.empty()) return name;
    if (!speedrunName.empty()) return speedrunName;
    return fallback;
}

Profile Profile::FromJson(const Json::Value& row) {
    Profile p;
    p.userId = row["user_id"].WStr();
    p.displayName = row["display_name"].WStr();
    p.avatar = row["avatar"].WStr();
    p.banner = row["banner"].WStr();
    p.bannerColor = ParseColor(row["banner_color"].Str(), p.bannerColor);
    if (row["name_font"].IsString()) p.font = row["name_font"].Str();
    if (row["name_effect"].IsString()) p.effect = row["name_effect"].Str();
    p.color1 = ParseColor(row["name_color"].Str(), p.color1);
    p.color2 = ParseColor(row["name_color2"].Str(), p.color2);
    p.cardColor = ParseColor(row["card_color"].Str(), kAutoColor);
    p.avatarDeco = Known(AvatarDecorations(), row["avatar_deco"].Str());
    p.frame = Known(ProfileFrames(), row["profile_frame"].Str());
    p.nameplate = Known(Nameplates(), row["nameplate"].Str());
    p.featuredBadge = row["featured_badge"].Str();
    p.speedrunId = row["speedrun_id"].WStr();
    p.speedrunName = row["speedrun_name"].WStr();
    p.speedrunUrl = row["speedrun_url"].WStr();
    p.speedrunAvatar = row["speedrun_avatar"].WStr();
    p.discordId = row["discord_id"].WStr();
    p.discordName = row["discord_name"].WStr();
    p.discordAvatar = row["discord_avatar"].WStr();
    for (const auto& [key, value] : row["playtime"].Items()) {
        if (auto chapter = Text::ToInt(key)) p.playtime[static_cast<int>(*chapter)] = value.Number();
    }
    return p;
}

Json::Value Profile::EditableJson() const {
    Json::Value o = Json::Value::MakeObject();
    o.Set("user_id", userId);
    o.Set("display_name", Text::Trim(displayName));
    o.Set("avatar", avatar);
    o.Set("banner", banner);
    o.Set("banner_color", ColorText(bannerColor));
    o.Set("name_font", font);
    o.Set("name_effect", effect);
    o.Set("name_color", ColorText(color1));
    o.Set("name_color2", ColorText(color2));
    o.Set("card_color", ColorText(cardColor));
    o.Set("avatar_deco", avatarDeco);
    o.Set("profile_frame", frame);
    o.Set("nameplate", nameplate);
    o.Set("featured_badge", featuredBadge);
    return o;
}

Json::Value Profile::CacheJson() const {
    Json::Value o = EditableJson();
    o.Set("speedrun_id", speedrunId);
    o.Set("speedrun_name", speedrunName);
    o.Set("speedrun_url", speedrunUrl);
    o.Set("speedrun_avatar", speedrunAvatar);
    o.Set("discord_id", discordId);
    o.Set("discord_name", discordName);
    o.Set("discord_avatar", discordAvatar);
    return o;
}

bool Profile::SameStyle(const Profile& o) const {
    return Text::Trim(displayName) == Text::Trim(o.displayName) && avatar == o.avatar && banner == o.banner && bannerColor == o.bannerColor &&
           font == o.font && effect == o.effect && color1 == o.color1 && color2 == o.color2 && cardColor == o.cardColor &&
           avatarDeco == o.avatarDeco && frame == o.frame && nameplate == o.nameplate &&
           featuredBadge == o.featuredBadge;
}

} // namespace SpeedrunLauncher::Services::Account
