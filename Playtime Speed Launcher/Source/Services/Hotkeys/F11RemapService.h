#pragma once
#include <functional>
#include <set>
#include <vector>
#include "Input/HotkeyBinding.h"

namespace SpeedrunLauncher::Services {

// While enabled, blocks the real F11 system-wide and lets the configured keys / mouse buttons
// act as F11 instead (f11remap.cfg: "enabled,K<vk>,M<button>,...").
class F11RemapService {
public:
    ~F11RemapService() { Uninstall(); }

    bool Enabled() const { return enabled; }
    const std::vector<Input::Binding>& Bindings() const { return bindings; }
    bool HasKeyboardHook() const { return keyboardId != 0; }

    // Every real (non-injected) key press the hook sees: (vk, time) — feeds the easter eggs.
    std::function<void(unsigned vk, unsigned time)> onRawKeyDown;

    void Load();
    void SetEnabled(bool on);
    void AddBinding(const Input::Binding& binding);      // ignores duplicates
    void ReplaceBinding(size_t index, const Input::Binding& binding);
    void RemoveBinding(size_t index);
    void Refresh();   // installs / removes the hooks to match the settings

private:
    void Save() const;
    void Uninstall();

    bool enabled = false;
    std::vector<Input::Binding> bindings;
    int keyboardId = 0, mouseId = 0;
};

} // namespace SpeedrunLauncher::Services
