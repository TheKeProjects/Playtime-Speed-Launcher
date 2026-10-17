#include "Settings/HeartbeatSettings.h"
#include <objbase.h>
#include <cstdio>
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"heartbeat_settings.json";
}

std::wstring NewGuid() {
    GUID g{};
    CoCreateGuid(&g);
    wchar_t buffer[40];
    swprintf_s(buffer, L"%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx", g.Data1, g.Data2, g.Data3,
               g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return buffer;
}

Heartbeat& Heartbeat::Current() {
    static Heartbeat current = [] {
        Heartbeat h;
        bool exists = false;
        const Json::Value json = Config::ReadJson(kFile, &exists);
        if (exists && json.IsObject()) {
            h.installId = json["InstallId"].WStr();
            h.showCounter = json["ShowCounter"].Bool(true);
        }
        if (h.installId.empty()) {
            h.installId = NewGuid();
            h.Save();
        }
        return h;
    }();
    return current;
}

void Heartbeat::Save() {
    Json::Value json = Json::Value::MakeObject();
    json.Set("InstallId", installId);
    json.Set("ShowCounter", showCounter);
    Config::WriteJson(kFile, json);
}

} // namespace SpeedrunLauncher::Settings
