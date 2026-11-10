#include "Settings/DuelSettings.h"
#include <algorithm>
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
            d.banTurn = json["BanTurn"].Bool(true);
            d.rivalFinished = json["RivalFinished"].Bool(true);
            d.leaves = json["Leaves"].Bool(true);
            d.roomChat = json["RoomChat"].Bool(true);
            d.directMessages = json["DirectMessages"].Bool(true);
            d.friendInvitesOnly = json["FriendInvitesOnly"].Bool(false);
            d.flashTaskbar = json["FlashTaskbar"].Bool(true);
            d.toastLength = static_cast<int>(std::clamp(json["ToastLength"].Int(1), 0LL, 2LL));
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
    json.Set("BanTurn", banTurn);
    json.Set("RivalFinished", rivalFinished);
    json.Set("Leaves", leaves);
    json.Set("RoomChat", roomChat);
    json.Set("DirectMessages", directMessages);
    json.Set("FriendInvitesOnly", friendInvitesOnly);
    json.Set("FlashTaskbar", flashTaskbar);
    json.Set("ToastLength", toastLength);
    Config::WriteJson(kFile, json);
}

} // namespace SpeedrunLauncher::Settings
