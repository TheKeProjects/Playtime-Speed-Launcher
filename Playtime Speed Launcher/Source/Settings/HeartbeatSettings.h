#pragma once
#include <string>

namespace SpeedrunLauncher::Settings {

// Per-install id used by the online counter heartbeat and the cloud playtime store.
// (heartbeat_settings.json: {"InstallId":"<guid>","ShowCounter":true})
struct Heartbeat {
    std::wstring installId;     // lower-case GUID with dashes
    bool showCounter = true;

    void Save();
    static Heartbeat& Current();
};

std::wstring NewGuid();

} // namespace SpeedrunLauncher::Settings
