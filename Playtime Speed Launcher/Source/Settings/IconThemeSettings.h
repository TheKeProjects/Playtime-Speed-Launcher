#pragma once
#include <string>

namespace SpeedrunLauncher::Settings {

// Launcher icon / Discord Rich Presence theme. "default" follows the mode baked into the build
// (AppVersion::BuildDefaultTheme); any other value overrides it. (icon_theme_settings.json)
struct IconTheme {
    std::wstring theme = L"default";  // default | classic | lgbtq | summer | halloween | christmas

    std::wstring EffectiveTheme() const;
    std::wstring DiscordImageKey() const;
    // Window icon resource id, or 0 to keep the exe's own icon ("default").
    int IconResourceId() const;

    void Save();
    static IconTheme& Current();
};

} // namespace SpeedrunLauncher::Settings
