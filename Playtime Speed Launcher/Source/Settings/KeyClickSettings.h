#pragma once
#include <vector>

namespace SpeedrunLauncher::Settings {

// "Key → click" (Settings → Controls): keyboard keys that become mouse clicks while the game has
// focus. Each one can instead spam clicks while held, at its own rate. Saved in key_click.json.
struct KeyClickBinding {
    unsigned vk = 0;
    int button = 0;          // 0 left, 1 right, 2 middle
    bool spam = false;       // hold = repeated clicks
    int clicksPerSecond = 15;
};

struct KeyClick {
    static constexpr int kMinCps = 2, kMaxCps = 40;

    bool enabled = false;
    bool everywhere = false;   // also outside the games (any app except the launcher itself)
    bool spamUnlocked = false; // the spam option is hidden until ↑↑↑↑↑↓↓↓↓↓ is typed in the launcher
    std::vector<KeyClickBinding> bindings;
    // Shortcut that turns the whole feature on / off from anywhere (RegisterHotKey); vk 0 = none
    unsigned toggleModifiers = 0;   // MOD_ALT / MOD_CONTROL / MOD_SHIFT / MOD_WIN
    unsigned toggleVk = 0;

    static KeyClick& Current();   // loaded on first use
    void Save() const;

private:
    void Load();
};

} // namespace SpeedrunLauncher::Settings
