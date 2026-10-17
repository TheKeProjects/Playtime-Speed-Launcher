#pragma once
#include <optional>
#include <string>

// Key naming used across the settings UI (identical to what the WPF launcher displayed).
namespace SpeedrunLauncher::Input::KeyNames {

// WPF Key enum name for a virtual-key ("D1", "NumPad5", "OemTilde", "Return"...).
std::wstring WpfName(unsigned vk);
// KeyToString(): friendlier names ("Enter", "Esc", "PgUp", arrows as ←→↑↓)
std::wstring Friendly(unsigned vk);
// "Ctrl + Shift + Enter"
std::wstring Hotkey(unsigned modifiers, unsigned vk);
// FormatKeyName(): WPF name upper-cased ("B", "OEMTILDE") — Cores tab buttons
std::wstring Upper(unsigned vk);
// Unreal Engine key name written to the mods' config.ini ("One", "SpaceBar", "F5"...), nullopt if unsupported
std::optional<std::wstring> Unreal(unsigned vk);
// Value stored in LoadManip / FullBright config.ini → what the UI shows ("XBUTTON_ONE" → "Mouse 4")
std::wstring ConfigValueDisplay(const std::wstring& configValue);

// Modifier keys are ignored while capturing a hotkey.
bool IsModifier(unsigned vk);

} // namespace SpeedrunLauncher::Input::KeyNames
