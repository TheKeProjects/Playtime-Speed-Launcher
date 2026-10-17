#include "Settings/IconThemeSettings.h"
#include "Resource.h"
#include "Services/App/AppVersion.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"icon_theme_settings.json";
}

IconTheme& IconTheme::Current() {
    static IconTheme current = [] {
        IconTheme t;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.Has("Theme")) t.theme = json["Theme"].WStr();
        return t;
    }();
    return current;
}

void IconTheme::Save() {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Theme", theme);
    Config::WriteJson(kFile, json);
}

std::wstring IconTheme::EffectiveTheme() const { return theme == L"default" ? AppVersion::BuildDefaultTheme() : theme; }

std::wstring IconTheme::DiscordImageKey() const {
    const std::wstring t = EffectiveTheme();
    if (t == L"lgbtq") return L"launcher_lgbtq";
    if (t == L"summer") return L"launcher_summer";
    if (t == L"halloween") return L"launcher_halloween";
    if (t == L"christmas") return L"launcher_christmas";
    return L"launcher";  // "classic" and unknown values use the plain icon
}

int IconTheme::IconResourceId() const {
    const std::wstring t = EffectiveTheme();
    if (t == L"classic") return IDI_ICON_CLASSIC;
    if (t == L"lgbtq") return IDI_ICON_LGBTQ;
    if (t == L"summer") return IDI_ICON_SUMMER;
    if (t == L"halloween") return IDI_ICON_HALLOWEEN;
    if (t == L"christmas") return IDI_ICON_CHRISTMAS;
    return 0;
}

} // namespace SpeedrunLauncher::Settings
