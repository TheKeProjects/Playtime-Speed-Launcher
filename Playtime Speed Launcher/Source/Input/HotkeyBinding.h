#pragma once
#include <optional>
#include <string>

namespace SpeedrunLauncher::Input {

enum class BindingType { None, Keyboard, Mouse };

// Mouse button ids shared by the remap features (persisted as "M1" / "M2" / "M3").
namespace MouseButtons {
inline constexpr int XButton1 = 1;  // "Mouse 4"
inline constexpr int XButton2 = 2;  // "Mouse 5"
inline constexpr int Middle = 3;    // "Mouse 3"
} // namespace MouseButtons

// A keyboard key or mouse button that triggers a remappable action.
struct Binding {
    BindingType type = BindingType::None;
    unsigned keyVk = 0;
    int mouseButton = 0;

    static Binding Key(unsigned vk) { return { BindingType::Keyboard, vk, 0 }; }
    static Binding Mouse(int button) { return { BindingType::Mouse, 0, button }; }
    bool operator==(const Binding& o) const { return type == o.type && keyVk == o.keyVk && mouseButton == o.mouseButton; }

    // "K73" / "M1"
    std::wstring Token() const;
    static std::optional<Binding> Parse(const std::wstring& token);
};

// Hotkey modifiers (RegisterHotKey MOD_* values)
inline constexpr unsigned ModAlt = 0x0001;
inline constexpr unsigned ModControl = 0x0002;
inline constexpr unsigned ModShift = 0x0004;
inline constexpr unsigned ModWin = 0x0008;

} // namespace SpeedrunLauncher::Input
