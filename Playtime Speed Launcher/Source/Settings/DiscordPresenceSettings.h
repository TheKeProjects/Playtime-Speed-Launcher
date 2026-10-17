#pragma once

namespace SpeedrunLauncher::Settings {

// discord_settings.json
struct DiscordPresence {
    bool showActivity = true;
    bool showVersion = true;
    bool showLiveSplit = false;

    void Save() const;
    static DiscordPresence& Current();
};

} // namespace SpeedrunLauncher::Settings
