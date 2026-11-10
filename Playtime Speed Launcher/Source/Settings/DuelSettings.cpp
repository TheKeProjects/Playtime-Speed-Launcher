#include "Settings/DuelSettings.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"duel_settings.json";
}

Duels& Duels::Current() {
    static Duels current = [] {
        Duels d;
        bool exists = false;
        const Json::Value json = Config::ReadJson(kFile, &exists);
        if (exists && json.IsObject()) {
            d.notifications = json["Notifications"].Bool(true);
            d.sounds = json["Sounds"].Bool(true);
            d.invites = json["Invites"].Bool(true);
            d.joins = json["Joins"].Bool(true);
            d.starts = json["Starts"].Bool(true);
            d.results = json["Results"].Bool(true);
        }
        return d;
    }();
    return current;
}

void Duels::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Notifications", notifications);
    json.Set("Sounds", sounds);
    json.Set("Invites", invites);
    json.Set("Joins", joins);
    json.Set("Starts", starts);
    json.Set("Results", results);
    Config::WriteJson(kFile, json);
}

} // namespace SpeedrunLauncher::Settings
