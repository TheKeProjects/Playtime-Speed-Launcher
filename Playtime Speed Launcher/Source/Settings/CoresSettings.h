#pragma once
#include "Input/HotkeyBinding.h"

namespace SpeedrunLauncher::Settings {

// "Cores" feature (process affinity load manipulation) and the Chapter 4 load remap.
struct Cores {
    // loadmanip_hotkeys.cfg — three "0,<vk>" lines: normal, slower, freeze
    unsigned freezeVk = 0x42;   // B
    unsigned slowerVk = 0x4E;   // N
    unsigned normalVk = 0x4D;   // M
    bool enabled = false;       // cores_enabled.cfg
    bool priorityHigh = true;   // cores_priority.cfg: "high" | "low"

    // chapter4_remap_hotkeys.cfg — three tokens (freeze, slow, normal); chapter4_remap_enabled.cfg
    Input::Binding chapter4Freeze = Input::Binding::Key(0x49);   // I
    Input::Binding chapter4Slow = Input::Binding::Key(0x4F);     // O
    Input::Binding chapter4Normal = Input::Binding::Key(0x50);   // P
    bool chapter4Enabled = false;

    void SaveHotkeys() const;
    void SaveEnabled() const;
    void SavePriority() const;
    void SaveChapter4Hotkeys() const;
    void SaveChapter4Enabled() const;

    static Cores& Current();
};

} // namespace SpeedrunLauncher::Settings
