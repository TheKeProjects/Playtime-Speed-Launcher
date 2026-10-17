#include "Input/HotkeyBinding.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Input {

std::wstring Binding::Token() const {
    if (type == BindingType::Mouse) return L"M" + std::to_wstring(mouseButton);
    return L"K" + std::to_wstring(keyVk);
}

std::optional<Binding> Binding::Parse(const std::wstring& token) {
    const std::wstring t = Text::Trim(token);
    if (t.size() < 2) return std::nullopt;
    const auto number = Text::ToInt(std::wstring_view(t).substr(1));
    if (!number || *number < 0) return std::nullopt;
    if (t[0] == L'K') return Key(static_cast<unsigned>(*number));
    if (t[0] == L'M') return Mouse(static_cast<int>(*number));
    return std::nullopt;
}

} // namespace SpeedrunLauncher::Input
