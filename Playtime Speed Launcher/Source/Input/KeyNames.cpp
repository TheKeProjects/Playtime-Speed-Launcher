#include "Input/KeyNames.h"
#include <windows.h>
#include "Core/Text.h"
#include "Input/HotkeyBinding.h"

namespace SpeedrunLauncher::Input::KeyNames {

std::wstring WpfName(unsigned vk) {
    if (vk >= '0' && vk <= '9') return L"D" + std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"NumPad" + std::to_wstring(vk - VK_NUMPAD0);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk) {
        case VK_BACK: return L"Back";
        case VK_TAB: return L"Tab";
        case VK_CLEAR: return L"Clear";
        case VK_RETURN: return L"Return";
        case VK_SHIFT: case VK_LSHIFT: return L"LeftShift";
        case VK_RSHIFT: return L"RightShift";
        case VK_CONTROL: case VK_LCONTROL: return L"LeftCtrl";
        case VK_RCONTROL: return L"RightCtrl";
        case VK_MENU: case VK_LMENU: return L"LeftAlt";
        case VK_RMENU: return L"RightAlt";
        case VK_PAUSE: return L"Pause";
        case VK_CAPITAL: return L"Capital";
        case VK_ESCAPE: return L"Escape";
        case VK_SPACE: return L"Space";
        case VK_PRIOR: return L"PageUp";
        case VK_NEXT: return L"Next";
        case VK_END: return L"End";
        case VK_HOME: return L"Home";
        case VK_LEFT: return L"Left";
        case VK_UP: return L"Up";
        case VK_RIGHT: return L"Right";
        case VK_DOWN: return L"Down";
        case VK_SELECT: return L"Select";
        case VK_PRINT: return L"Print";
        case VK_EXECUTE: return L"Execute";
        case VK_SNAPSHOT: return L"Snapshot";
        case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete";
        case VK_HELP: return L"Help";
        case VK_LWIN: return L"LWin";
        case VK_RWIN: return L"RWin";
        case VK_APPS: return L"Apps";
        case VK_SLEEP: return L"Sleep";
        case VK_MULTIPLY: return L"Multiply";
        case VK_ADD: return L"Add";
        case VK_SEPARATOR: return L"Separator";
        case VK_SUBTRACT: return L"Subtract";
        case VK_DECIMAL: return L"Decimal";
        case VK_DIVIDE: return L"Divide";
        case VK_NUMLOCK: return L"NumLock";
        case VK_SCROLL: return L"Scroll";
        case VK_BROWSER_BACK: return L"BrowserBack";
        case VK_BROWSER_FORWARD: return L"BrowserForward";
        case VK_BROWSER_REFRESH: return L"BrowserRefresh";
        case VK_BROWSER_STOP: return L"BrowserStop";
        case VK_BROWSER_SEARCH: return L"BrowserSearch";
        case VK_BROWSER_FAVORITES: return L"BrowserFavorites";
        case VK_BROWSER_HOME: return L"BrowserHome";
        case VK_VOLUME_MUTE: return L"VolumeMute";
        case VK_VOLUME_DOWN: return L"VolumeDown";
        case VK_VOLUME_UP: return L"VolumeUp";
        case VK_MEDIA_NEXT_TRACK: return L"MediaNextTrack";
        case VK_MEDIA_PREV_TRACK: return L"MediaPreviousTrack";
        case VK_MEDIA_STOP: return L"MediaStop";
        case VK_MEDIA_PLAY_PAUSE: return L"MediaPlayPause";
        case VK_LAUNCH_MAIL: return L"LaunchMail";
        case VK_LAUNCH_MEDIA_SELECT: return L"SelectMedia";
        case VK_LAUNCH_APP1: return L"LaunchApplication1";
        case VK_LAUNCH_APP2: return L"LaunchApplication2";
        case VK_OEM_1: return L"OemSemicolon";
        case VK_OEM_PLUS: return L"OemPlus";
        case VK_OEM_COMMA: return L"OemComma";
        case VK_OEM_MINUS: return L"OemMinus";
        case VK_OEM_PERIOD: return L"OemPeriod";
        case VK_OEM_2: return L"OemQuestion";
        case VK_OEM_3: return L"OemTilde";
        case VK_OEM_4: return L"OemOpenBrackets";
        case VK_OEM_5: return L"OemPipe";
        case VK_OEM_6: return L"OemCloseBrackets";
        case VK_OEM_7: return L"OemQuotes";
        case VK_OEM_8: return L"Oem8";
        case VK_OEM_102: return L"OemBackslash";
        default: return L"None";
    }
}

std::wstring Friendly(unsigned vk) {
    switch (vk) {
        case VK_RETURN: return L"Enter";
        case VK_BACK: return L"Backspace";
        case VK_ESCAPE: return L"Esc";
        case VK_SPACE: return L"Space";
        case VK_PRIOR: return L"PgUp";
        case VK_NEXT: return L"PgDn";
        case VK_DELETE: return L"Del";
        case VK_INSERT: return L"Ins";
        case VK_LEFT: return L"\x2190";
        case VK_RIGHT: return L"\x2192";
        case VK_UP: return L"\x2191";
        case VK_DOWN: return L"\x2193";
        default: return WpfName(vk);
    }
}

std::wstring Hotkey(unsigned modifiers, unsigned vk) {
    std::wstring out;
    const auto add = [&](const wchar_t* part) {
        if (!out.empty()) out += L" + ";
        out += part;
    };
    if (modifiers & ModControl) add(L"Ctrl");
    if (modifiers & ModAlt) add(L"Alt");
    if (modifiers & ModShift) add(L"Shift");
    if (modifiers & ModWin) add(L"Win");
    add(Friendly(vk).c_str());
    return out;
}

std::wstring Upper(unsigned vk) { return Text::Upper(WpfName(vk)); }

std::optional<std::wstring> Unreal(unsigned vk) {
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    static const wchar_t* digits[] = { L"Zero", L"One", L"Two", L"Three", L"Four", L"Five", L"Six", L"Seven", L"Eight", L"Nine" };
    if (vk >= '0' && vk <= '9') return std::wstring(digits[vk - '0']);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"NumPad" + std::wstring(digits[vk - VK_NUMPAD0]);
    switch (vk) {
        case VK_MULTIPLY: return L"Multiply";
        case VK_ADD: return L"Add";
        case VK_SUBTRACT: return L"Subtract";
        case VK_DIVIDE: return L"Divide";
        case VK_DECIMAL: return L"Decimal";
        case VK_LEFT: return L"Left";
        case VK_RIGHT: return L"Right";
        case VK_UP: return L"Up";
        case VK_DOWN: return L"Down";
        case VK_SPACE: return L"SpaceBar";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_BACK: return L"BackSpace";
        case VK_DELETE: return L"Delete";
        case VK_INSERT: return L"Insert";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"PageUp";
        case VK_NEXT: return L"PageDown";
        case VK_CAPITAL: return L"CapsLock";
        case VK_NUMLOCK: return L"NumLock";
        case VK_SCROLL: return L"ScrollLock";
        case VK_PAUSE: return L"Pause";
        case VK_OEM_COMMA: return L"Comma";
        case VK_OEM_PERIOD: return L"Period";
        case VK_OEM_2: return L"Slash";
        case VK_OEM_1: return L"Semicolon";
        case VK_OEM_7: return L"Quote";
        case VK_OEM_4: return L"LeftBracket";
        case VK_OEM_6: return L"RightBracket";
        case VK_OEM_5: return L"Backslash";
        case VK_OEM_MINUS: return L"Hyphen";
        case VK_OEM_PLUS: return L"Equals";
        case VK_OEM_3: return L"Tilde";
        default: return std::nullopt;
    }
}

std::wstring ConfigValueDisplay(const std::wstring& value) {
    if (value == L"XBUTTON_ONE") return L"Mouse 4";
    if (value == L"XBUTTON_TWO") return L"Mouse 5";
    if (value == L"MIDDLE_MOUSE_BUTTON") return L"Middle Mouse";
    return value;
}

bool IsModifier(unsigned vk) {
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU: case VK_LMENU: case VK_RMENU:
        case VK_LWIN: case VK_RWIN:
        case 0: case VK_PROCESSKEY:
            return true;
        default:
            return false;
    }
}

} // namespace SpeedrunLauncher::Input::KeyNames
