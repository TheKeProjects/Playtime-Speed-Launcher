#pragma once

namespace SpeedrunLauncher::Settings {

// Global hotkeys (RegisterHotKey) and the sound effects volume.
//   hotkey.cfg           "modifiers,vk"   checkpoint overlay (default Ctrl+Shift+Enter)
//   tutorial_hotkey.cfg  "modifiers,vk"   video tutorial overlay (default F9)
//   volume.cfg           "0.5000"
struct Hotkeys {
    unsigned checkpointModifiers = 0x0002 | 0x0004;  // Ctrl + Shift
    unsigned checkpointVk = 0x0D;                    // Enter
    unsigned tutorialModifiers = 0;
    unsigned tutorialVk = 0x78;                      // F9
    float volume = 0.5f;

    void SaveCheckpoint() const;
    void SaveTutorial() const;
    void SaveVolume() const;
    static Hotkeys& Current();
};

} // namespace SpeedrunLauncher::Settings
