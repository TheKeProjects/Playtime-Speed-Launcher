#include "Settings/WindowModeSettings.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"window_mode_settings.json";
}

WindowMode& WindowMode::Current() {
    static WindowMode current = [] {
        WindowMode m;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.Has("Mode")) m.mode = json["Mode"].WStr();
        return m;
    }();
    return current;
}

void WindowMode::Save() {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Mode", mode);
    Config::WriteJson(kFile, json);
}

} // namespace SpeedrunLauncher::Settings
