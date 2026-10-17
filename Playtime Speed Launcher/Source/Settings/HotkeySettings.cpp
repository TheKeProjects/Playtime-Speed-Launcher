#include "Settings/HotkeySettings.h"
#include <algorithm>
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {

bool ReadPair(const wchar_t* file, unsigned& modifiers, unsigned& vk) {
    auto text = Config::ReadText(file);
    if (!text) return false;
    const auto parts = Text::Split(*text, L',');
    if (parts.size() != 2) return false;
    const auto mod = Text::ToInt(parts[0]), key = Text::ToInt(parts[1]);
    if (!mod || !key || *mod < 0 || *key < 0) return false;
    modifiers = static_cast<unsigned>(*mod);
    vk = static_cast<unsigned>(*key);
    return true;
}

} // namespace

Hotkeys& Hotkeys::Current() {
    static Hotkeys current = [] {
        Hotkeys h;
        ReadPair(L"hotkey.cfg", h.checkpointModifiers, h.checkpointVk);
        ReadPair(L"tutorial_hotkey.cfg", h.tutorialModifiers, h.tutorialVk);
        if (auto v = Config::ReadText(L"volume.cfg")) {
            if (auto d = Text::ToDouble(*v)) h.volume = std::clamp(static_cast<float>(*d), 0.f, 1.f);
        }
        return h;
    }();
    return current;
}

void Hotkeys::SaveCheckpoint() const {
    Config::WriteText(L"hotkey.cfg", std::to_wstring(checkpointModifiers) + L"," + std::to_wstring(checkpointVk));
}

void Hotkeys::SaveTutorial() const {
    Config::WriteText(L"tutorial_hotkey.cfg", std::to_wstring(tutorialModifiers) + L"," + std::to_wstring(tutorialVk));
}

void Hotkeys::SaveVolume() const { Config::WriteText(L"volume.cfg", Text::FromDouble(volume, 4)); }

} // namespace SpeedrunLauncher::Settings
