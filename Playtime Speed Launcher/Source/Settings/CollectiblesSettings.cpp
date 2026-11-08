#include "Settings/CollectiblesSettings.h"
#include <algorithm>
#include <cstdio>
#include "Localization/Localization.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"collectibles_settings.json";
}

Collectibles& Collectibles::Current() {
    static Collectibles current = [] {
        Collectibles c;
        c.english = Loc::CurrentLang() != L"es";   // first run: the launcher's language
        bool exists = false;
        const Json::Value json = Config::ReadJson(kFile, &exists);
        if (exists && json.IsObject()) {
            const long long ch = json["Chapter"].Int(0);
            c.chapter = ch >= 1 && ch <= 5 ? static_cast<int>(ch) : 0;
            c.counters = json["Counters"].Bool(true);
            c.list = json["List"].Bool(false);
            c.missingOnly = json["MissingOnly"].Bool(true);
            c.toasts = json["Toasts"].Bool(true);
            c.animations = json["Animations"].Bool(true);
            c.noBackground = json["NoBackground"].Bool(false);
            c.right = json["Right"].Bool(false);
            c.english = json["English"].Bool(c.english);
            c.scale = std::clamp(json["Scale"].Number(1.0), 0.5, 2.0);
            c.listColumns = static_cast<int>(std::clamp(json["ListColumns"].Int(1), 1LL, 3LL));
            c.inGame = json["InGame"].Bool(false);
            c.inGameOffset = static_cast<int>(std::max(0LL, json["InGameOffset"].Int(0)));
            c.resetVk = static_cast<unsigned>(std::clamp(json["ResetVk"].Int(c.resetVk), 0LL, 255LL));
        }
        return c;
    }();
    return current;
}

void Collectibles::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Chapter", static_cast<double>(chapter));
    json.Set("Counters", counters);
    json.Set("List", list);
    json.Set("MissingOnly", missingOnly);
    json.Set("Toasts", toasts);
    json.Set("Animations", animations);
    json.Set("NoBackground", noBackground);
    json.Set("Right", right);
    json.Set("English", english);
    json.Set("Scale", scale);
    json.Set("ListColumns", static_cast<double>(listColumns));
    json.Set("InGame", inGame);
    json.Set("InGameOffset", inGameOffset);
    json.Set("ResetVk", resetVk);
    Config::WriteJson(kFile, json);
}

std::string Collectibles::Query() const {
    std::string q = "view=panel";
    if (chapter >= 1 && chapter <= 5) q += "&ch=" + std::to_string(chapter);
    std::string show;
    if (counters) show = "cats";
    if (list) show += show.empty() ? "list" : ",list";
    q += "&show=" + show;
    if (list && missingOnly) q += "&missing=1";
    if (noBackground) q += "&bg=0";
    if (right) q += "&align=right";
    if (listColumns > 1) q += "&lcols=" + std::to_string(listColumns);
    if (!toasts) q += "&toasts=0";
    if (!animations) q += "&anim=0";
    if (english) q += "&lang=en";
    if (scale != 1.0) {
        char buffer[16];
        sprintf_s(buffer, "%g", scale);
        q += "&scale=";
        q += buffer;
    }
    return q;
}

} // namespace SpeedrunLauncher::Settings
