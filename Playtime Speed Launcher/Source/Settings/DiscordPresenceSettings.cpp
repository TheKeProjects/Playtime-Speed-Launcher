#include "Settings/DiscordPresenceSettings.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"discord_settings.json";
}

DiscordPresence& DiscordPresence::Current() {
    static DiscordPresence current = [] {
        DiscordPresence d;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.IsObject()) {
            d.showActivity = json["ShowActivity"].Bool(true);
            d.showVersion = json["ShowVersion"].Bool(true);
            d.showLiveSplit = json["ShowLiveSplit"].Bool(false);
        }
        return d;
    }();
    return current;
}

void DiscordPresence::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("ShowActivity", showActivity);
    json.Set("ShowVersion", showVersion);
    json.Set("ShowLiveSplit", showLiveSplit);
    Config::WriteJson(kFile, json);
}

} // namespace SpeedrunLauncher::Settings
