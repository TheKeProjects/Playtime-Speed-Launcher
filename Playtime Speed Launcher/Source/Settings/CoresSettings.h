#pragma once
#include "Input/HotkeyBinding.h"

namespace SpeedrunLauncher::Settings {

// "Cores" feature (process affinity load manipulation).
struct Cores {
    // loadmanip_hotkeys.cfg — three "0,<vk>" lines: normal, slower, freeze
    unsigned freezeVk = 0x42;   // B
    unsigned slowerVk = 0x4E;   // N
    unsigned normalVk = 0x4D;   // M
    bool enabled = false;       // cores_enabled.cfg
    bool priorityHigh = true;   // cores_priority.cfg: "high" | "low"

    void SaveHotkeys() const;
    void SaveEnabled() const;
    void SavePriority() const;

    static Cores& Current();
};

} // namespace SpeedrunLauncher::Settings
