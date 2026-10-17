#include "Settings/KeyClickSettings.h"
#include <algorithm>
#include "Core/Json.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"key_click.json";
}

KeyClick& KeyClick::Current() {
    static KeyClick settings = [] {
        KeyClick s;
        s.Load();
        return s;
    }();
    return settings;
}

void KeyClick::Load() {
    const Json::Value root = Config::ReadJson(kFile);   // kept alive: Elements() refers into it
    enabled = root["Enabled"].Bool();
    everywhere = root["Everywhere"].Bool();
    spamUnlocked = root["SpamUnlocked"].Bool();
    toggleModifiers = static_cast<unsigned>(root["ToggleModifiers"].Number());
    toggleVk = static_cast<unsigned>(root["ToggleVk"].Number());
    bindings.clear();
    for (const auto& item : root["Bindings"].Elements()) {
        KeyClickBinding b;
        b.vk = static_cast<unsigned>(item["Vk"].Number());
        b.button = std::clamp(static_cast<int>(item["Button"].Number()), 0, 2);
        b.spam = item["Spam"].Bool();
        b.clicksPerSecond = std::clamp(static_cast<int>(item["ClicksPerSecond"].Number(15.0)), kMinCps, kMaxCps);
        if (b.vk) bindings.push_back(b);
    }
}

void KeyClick::Save() const {
    Json::Value root = Json::Value::MakeObject();
    root.Set("Enabled", enabled);
    root.Set("Everywhere", everywhere);
    root.Set("SpamUnlocked", spamUnlocked);
    root.Set("ToggleModifiers", toggleModifiers);
    root.Set("ToggleVk", toggleVk);
    Json::Value list = Json::Value::MakeArray();
    for (const auto& b : bindings) {
        Json::Value item = Json::Value::MakeObject();
        item.Set("Vk", static_cast<int>(b.vk));
        item.Set("Button", b.button);
        item.Set("Spam", b.spam);
        item.Set("ClicksPerSecond", b.clicksPerSecond);
        list.Push(item);
    }
    root.Set("Bindings", list);
    Config::WriteJson(kFile, root, true);
}

} // namespace SpeedrunLauncher::Settings
